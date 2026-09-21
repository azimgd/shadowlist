import { useCallback, useMemo, useRef, useState, type ReactNode } from 'react';
import { Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';
import {
  ShadowListNative,
  type ShadowListNativeCommands,
  type ShadowListNativeElementPressEvent,
} from 'shadowlist';
import { useInfiniteListProps } from 'shadowlist-utils';
import {
  ListFooter,
  ListHeader,
  Spinner,
  createStyles,
  defaultFeedLabels,
  formatRelativeTime,
  useTheme,
  type FeedItem,
} from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { useFeedQuery, usePublishPosts, useRefreshFeed } from './queries/feed';
import { generateFeedElement } from './fixtures/feed';

const PUBLISH_COUNT = 10;
const AVATAR_SIZE = 40;
const GALLERY_SLOTS = 4;
const SINGLE_IMAGE_HEIGHT = 200;

/*
 * A Feed row flattened into plain data for the native templates: everything the row shows is a
 * field, computed once here rather than in a render per row.
 */
interface FeedNativeRow {
  id: string;
  type: 'text' | 'image' | 'gallery';
  name: string;
  handle: string;
  date: string;
  text: string;
  initials: string;
  avatarColor: string;
  images: string[];
  liked: boolean;
  likes: number;
  likeLabel: string;
  likeColor: string;
}

function getInitials(name: string): string {
  const words = name.trim().split(/\s+/).filter(Boolean);
  const first = words[0] ?? '';
  const last = words.length > 1 ? words[words.length - 1]! : '';
  return `${[...first][0] ?? ''}${[...last][0] ?? ''}`.toUpperCase();
}

function getAvatarColor(name: string, palette: ReadonlyArray<string>): string {
  let hash = 0;
  for (let index = 0; index < name.length; index++) {
    hash = (hash * 31 + name.charCodeAt(index)) % 2147483647;
  }
  return palette[hash % palette.length] ?? '#888888';
}

function likeFields(
  liked: boolean,
  likes: number,
  accent: string,
  muted: string
) {
  return {
    liked,
    likes,
    likeLabel: `${liked ? '♥' : '♡'} ${likes}`,
    likeColor: liked ? accent : muted,
  };
}

function toRow(
  item: FeedItem,
  palette: ReadonlyArray<string>,
  accent: string,
  muted: string
): FeedNativeRow {
  const images = (item.images ?? []).map((image) => image.uri);
  const likes = item.id.charCodeAt(item.id.length - 1) % 40;
  return {
    id: item.id,
    type:
      images.length === 0 ? 'text' : images.length === 1 ? 'image' : 'gallery',
    name: item.author.name,
    handle: item.author.handle ?? '',
    date:
      item.createdAt === undefined
        ? ''
        : `· ${formatRelativeTime(item.createdAt, defaultFeedLabels)}`,
    text: item.text ?? '',
    initials: getInitials(item.author.name),
    avatarColor:
      item.author.avatarColor ?? getAvatarColor(item.author.name, palette),
    images,
    ...likeFields(false, likes, accent, muted),
  };
}

export const FeedNativeScreen = () => {
  const screenStyles = useScreenStyles();
  const styles = useStyles();
  const { colors } = useTheme();
  const listRef = useRef<ShadowListNativeCommands<FeedNativeRow>>(null);
  const [nameAccent, setNameAccent] = useState(false);
  const publishedRef = useRef(0);

  const feed = useFeedQuery();
  const refreshFeed = useRefreshFeed();
  const list = useInfiniteListProps(feed, { refresh: refreshFeed });
  const { mutate: publishPosts } = usePublishPosts();

  const rows = useMemo(
    () =>
      list.data.map((item) =>
        toRow(item, colors.avatarPalette, colors.accent, colors.secondaryLabel)
      ),
    [list.data, colors]
  );

  useHeaderActions({
    onPrepend: () => publishPosts(PUBLISH_COUNT),
    onAppend: list.onEndReached,
    onScrollToRandom: () =>
      listRef.current?.scrollToIndex(
        Math.floor(Math.random() * (listRef.current?.getCount() ?? 1))
      ),
  });

  const handleElementPress = useCallback(
    ({
      key,
      action,
      item,
    }: ShadowListNativeElementPressEvent<FeedNativeRow>) => {
      if (!item) return;
      if (action === 'like') {
        const liked = !item.liked;
        listRef.current?.updateItem(
          key,
          likeFields(
            liked,
            item.likes + (liked ? 1 : -1),
            colors.accent,
            colors.secondaryLabel
          )
        );
      } else if (action === 'hide') {
        listRef.current?.removeItems([key]);
      }
    },
    [colors]
  );

  const prependLocal = useCallback(() => {
    const fresh = Array.from({ length: 3 }, () => {
      publishedRef.current += 1;
      return toRow(
        generateFeedElement(publishedRef.current),
        colors.avatarPalette,
        colors.accent,
        colors.secondaryLabel
      );
    });
    listRef.current?.prependItems(fresh);
  }, [colors]);

  const removeFirst = useCallback(() => {
    const first = listRef.current?.getKeys()[0];
    if (first !== undefined) listRef.current?.removeItems([first]);
  }, []);

  const toggleAccent = useCallback(() => {
    const next = !nameAccent;
    setNameAccent(next);
    const style = next ? { color: colors.accent } : null;
    for (const template of ['text', 'image', 'gallery']) {
      listRef.current?.setTemplateStyle(template, 'name', style);
    }
  }, [nameAccent, colors]);

  const likeRandom = useCallback(() => {
    const keys = listRef.current?.getKeys() ?? [];
    const key = keys[Math.floor(Math.random() * Math.min(keys.length, 6))];
    const item = key === undefined ? undefined : listRef.current?.getItem(key);
    if (key === undefined || !item) return;
    const liked = !item.liked;
    listRef.current?.updateItem(key, {
      ...likeFields(
        liked,
        item.likes + (liked ? 1 : -1),
        colors.accent,
        colors.secondaryLabel
      ),
      text: `${item.text.replace(/ \(edited\)$/, '')} (edited)`,
    });
  }, [colors]);

  const templates = useMemo(() => {
    const avatar = (
      <ShadowListNative.View
        style={styles.avatar}
        bind={{ backgroundColor: 'avatarColor' }}
      >
        <ShadowListNative.Text
          style={styles.initials}
          bind={{ text: 'initials' }}
        />
      </ShadowListNative.View>
    );
    const meta = (
      <View style={styles.header}>
        <ShadowListNative.Text
          id="name"
          style={styles.name}
          numberOfLines={1}
          bind={{ text: 'name' }}
        />
        <ShadowListNative.Text
          style={styles.secondary}
          numberOfLines={1}
          bind={{ text: 'handle', hidden: '!handle' }}
        />
        <ShadowListNative.Text
          style={styles.date}
          numberOfLines={1}
          bind={{ text: 'date', hidden: '!date' }}
        />
      </View>
    );
    const actions = (
      <View style={styles.actions}>
        <ShadowListNative.View action="like" style={styles.actionHit}>
          <ShadowListNative.Text
            style={styles.action}
            bind={{ text: 'likeLabel', color: 'likeColor' }}
          />
        </ShadowListNative.View>
        <ShadowListNative.View action="hide" style={styles.actionHit}>
          <Text style={styles.action}>Hide</Text>
        </ShadowListNative.View>
      </View>
    );
    const row = (body: ReactNode) => (
      <ShadowListNative.View id="row" style={styles.row}>
        {avatar}
        <View style={styles.content}>
          {meta}
          {body}
          {actions}
        </View>
        <View style={styles.separator} />
      </ShadowListNative.View>
    );
    return {
      text: row(
        <ShadowListNative.Text style={styles.text} bind={{ text: 'text' }} />
      ),
      image: row(
        <>
          <ShadowListNative.Text
            style={[styles.text, styles.textAboveImages]}
            bind={{ text: 'text', hidden: '!text' }}
          />
          <View style={[styles.imageFrame, styles.singleImage]}>
            <ShadowListNative.Image
              style={styles.image}
              resizeMode="cover"
              bind={{ uri: 'images.0' }}
            />
          </View>
        </>
      ),
      gallery: row(
        <>
          <ShadowListNative.Text
            style={[styles.text, styles.textAboveImages]}
            bind={{ text: 'text', hidden: '!text' }}
          />
          <ScrollView
            horizontal
            showsHorizontalScrollIndicator={false}
            style={styles.strip}
            contentContainerStyle={styles.stripContent}
          >
            {Array.from({ length: GALLERY_SLOTS }, (_, slot) => (
              <ShadowListNative.View
                key={slot}
                style={[styles.imageFrame, styles.stripImage]}
                bind={{ hidden: `!images.${slot}` }}
              >
                <ShadowListNative.Image
                  style={styles.image}
                  resizeMode="cover"
                  bind={{ uri: `images.${slot}` }}
                />
              </ShadowListNative.View>
            ))}
          </ScrollView>
        </>
      ),
    };
  }, [styles]);

  const { hasNextPage } = feed;
  const footer = useMemo(
    () =>
      hasNextPage ? <Spinner /> : <ListFooter text="You're all caught up" />,
    [hasNextPage]
  );
  const listHeader = useMemo(
    () => (
      <ListHeader
        title="Skyfy (Native)"
        subtitle="Rows cloned natively from templates"
      />
    ),
    []
  );

  if (feed.data === undefined) {
    return <QueryStatus error={feed.error} onRetry={feed.refetch} />;
  }

  return (
    <View style={screenStyles.container}>
      <ShadowListNative
        ref={listRef}
        data={rows}
        templates={templates}
        templateKey="type"
        style={screenStyles.list}
        autoHideHeader
        refreshing={list.refreshing}
        onRefresh={list.onRefresh}
        refreshColor={colors.secondaryLabel}
        onEndReached={list.onEndReached}
        onElementPress={handleElementPress}
        ListHeaderComponent={listHeader}
        ListFooterComponent={footer}
      />
      <View style={styles.toolbar}>
        <ToolbarButton label="Prepend" onPress={prependLocal} />
        <ToolbarButton label="Remove top" onPress={removeFirst} />
        <ToolbarButton label="Update" onPress={likeRandom} />
        <ToolbarButton
          label={nameAccent ? 'Plain names' : 'Accent names'}
          onPress={toggleAccent}
        />
      </View>
    </View>
  );
};

const ToolbarButton = ({
  label,
  onPress,
}: {
  label: string;
  onPress: () => void;
}) => {
  const styles = useStyles();
  return (
    <Pressable
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={label}
      style={({ pressed }) => [styles.button, pressed && styles.pressed]}
    >
      <Text style={styles.buttonText}>{label}</Text>
    </Pressable>
  );
};

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    row: {
      backgroundColor: theme.colors.background,
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: theme.spacing.md,
      flexDirection: 'row',
    },
    avatar: {
      width: AVATAR_SIZE,
      height: AVATAR_SIZE,
      borderRadius: AVATAR_SIZE / 2,
      alignItems: 'center',
      justifyContent: 'center',
      overflow: 'hidden',
      marginRight: theme.spacing.md,
    },
    initials: {
      color: theme.colors.label,
      fontWeight: theme.fontWeight.semibold,
      fontSize: Math.floor(AVATAR_SIZE * 0.43),
    },
    content: {
      flex: 1,
    },
    header: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
      marginBottom: theme.spacing.xxs,
    },
    name: {
      color: theme.colors.label,
      ...theme.typography.subhead,
      fontWeight: theme.fontWeight.semibold,
      flexShrink: 1,
    },
    secondary: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
      flexShrink: 1,
    },
    date: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
    },
    text: {
      color: theme.colors.label,
      ...theme.typography.subhead,
    },
    textAboveImages: {
      marginBottom: theme.spacing.md,
    },
    imageFrame: {
      borderRadius: theme.radius.lg,
      overflow: 'hidden',
      backgroundColor: theme.colors.elevated2,
    },
    singleImage: {
      height: SINGLE_IMAGE_HEIGHT,
    },
    image: {
      width: '100%',
      height: '100%',
    },
    strip: {
      marginHorizontal: -theme.spacing.xs,
    },
    stripContent: {
      paddingHorizontal: theme.spacing.xs,
    },
    stripImage: {
      width: 280,
      height: SINGLE_IMAGE_HEIGHT,
      marginHorizontal: theme.spacing.xs,
    },
    actions: {
      flexDirection: 'row',
      marginTop: theme.spacing.sm,
    },
    actionHit: {
      paddingVertical: theme.spacing.xxs,
      paddingRight: theme.spacing.md,
    },
    action: {
      ...theme.typography.footnote,
      color: theme.colors.secondaryLabel,
    },
    separator: {
      position: 'absolute',
      left: theme.rowInset,
      right: 0,
      bottom: 0,
      height: StyleSheet.hairlineWidth,
      backgroundColor: theme.colors.separator,
    },
    toolbar: {
      position: 'absolute',
      left: theme.spacing.lg,
      right: theme.spacing.lg,
      bottom: theme.spacing.xl,
      flexDirection: 'row',
      justifyContent: 'space-between',
      gap: theme.spacing.xs,
      padding: theme.spacing.xs,
      borderRadius: theme.radius.lg,
      backgroundColor: theme.colors.elevated2,
    },
    button: {
      flex: 1,
      paddingVertical: theme.spacing.sm,
      borderRadius: theme.radius.md,
      alignItems: 'center',
    },
    pressed: {
      opacity: 0.4,
    },
    buttonText: {
      ...theme.typography.footnote,
      color: theme.colors.accent,
      fontWeight: theme.fontWeight.semibold,
    },
  })
);
