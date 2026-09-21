import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import { Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';
import {
  ShadowListNative,
  type ShadowListNativeCommands,
  type ShadowListNativeElementPressEvent,
} from 'shadowlist';
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
import { fetchFeedPage, publishPosts } from './api/feed';
import type { CursorPage } from './api/Collection';
import { generateFeedElement } from './fixtures/feed';

const PUBLISH_COUNT = 10;
const AVATAR_SIZE = 40;
const GALLERY_SLOTS = 4;
const SINGLE_IMAGE_HEIGHT = 200;

/*
 * A Feed row flattened into plain data for the native templates: everything the row shows is a
 * field, computed once here rather than in a render per row. Only content: theme colors are
 * template styles, so a theme switch restyles the rows without touching the data.
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
}

// The reader's own changes, which the fake server does not know: applied to every page that
// arrives (paging, refresh), as a real app's local mutation cache would.
interface LocalEdits {
  hidden: Set<string>;
  patches: Map<string, Partial<FeedNativeRow>>;
  // Rows made on this screen ("Prepend"): kept at the top across a refresh.
  created: FeedNativeRow[];
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

function toRow(item: FeedItem, palette: ReadonlyArray<string>): FeedNativeRow {
  const images = (item.images ?? []).map((image) => image.uri);
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
    liked: false,
    likes: item.id.charCodeAt(item.id.length - 1) % 40,
  };
}

function toggleLike(row: FeedNativeRow): Partial<FeedNativeRow> {
  return { liked: !row.liked, likes: row.likes + (row.liked ? -1 : 1) };
}

/*
 * Feed on ShadowListNative. The list owns its rows (`initialData`): the first page seeds it,
 * later pages are appended, likes/hides/prepends are commands, and a pull to refresh replaces
 * the rows with setData once the spinner has retracted (so the new posts show at the top).
 */
export const FeedNativeScreen = () => {
  const screenStyles = useScreenStyles();
  const styles = useStyles();
  const { colors } = useTheme();
  const listRef = useRef<ShadowListNativeCommands<FeedNativeRow>>(null);
  const [nameAccent, setNameAccent] = useState(false);
  const publishedRef = useRef(0);

  // The avatar palette is content (the same in both themes); read it without a dependency.
  const paletteRef = useRef(colors.avatarPalette);
  paletteRef.current = colors.avatarPalette;

  const [initialRows, setInitialRows] = useState<FeedNativeRow[] | null>(null);
  const [error, setError] = useState<Error | null>(null);
  const [hasNextPage, setHasNextPage] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const cursorRef = useRef<number | undefined>(undefined);
  const loadingRef = useRef(false);
  const heldPageRef = useRef<CursorPage<FeedItem> | null>(null);
  // Bumped when a refresh replaces the rows; a page requested before that is dropped.
  const generationRef = useRef(0);
  const editsRef = useRef<LocalEdits>({
    hidden: new Set(),
    patches: new Map(),
    created: [],
  });

  const edit = useCallback((key: string, patch: Partial<FeedNativeRow>) => {
    const { patches } = editsRef.current;
    patches.set(key, { ...patches.get(key), ...patch });
    listRef.current?.updateItem(key, patch);
  }, []);

  const hide = useCallback((keys: string[]) => {
    for (const key of keys) editsRef.current.hidden.add(key);
    listRef.current?.removeItems(keys);
  }, []);

  const rowsOf = useCallback((items: ReadonlyArray<FeedItem>) => {
    const { hidden, patches } = editsRef.current;
    return items
      .filter((item) => !hidden.has(item.id))
      .map((item) => ({
        ...toRow(item, paletteRef.current),
        ...patches.get(item.id),
      }));
  }, []);

  const takePage = useCallback((page: CursorPage<FeedItem>) => {
    cursorRef.current = page.nextCursor;
    setHasNextPage(page.nextCursor !== undefined);
  }, []);

  const loadFirstPage = useCallback(() => {
    setError(null);
    fetchFeedPage(undefined).then((page) => {
      takePage(page);
      setInitialRows(rowsOf(page.items));
    }, setError);
  }, [rowsOf, takePage]);

  useEffect(loadFirstPage, [loadFirstPage]);

  const loadMore = useCallback(() => {
    const cursor = cursorRef.current;
    if (loadingRef.current || cursor === undefined) return;
    loadingRef.current = true;
    const generation = generationRef.current;
    fetchFeedPage({ after: cursor })
      .then((page) => {
        // It continues the rows a refresh has since replaced: appending it would skip posts.
        if (generation !== generationRef.current) return;
        takePage(page);
        listRef.current?.appendItems(rowsOf(page.items));
      })
      .catch(() => {})
      .finally(() => {
        loadingRef.current = false;
      });
  }, [rowsOf, takePage]);

  const handleRefresh = useCallback(() => {
    setRefreshing(true);
    fetchFeedPage(undefined)
      .then((page) => {
        heldPageRef.current = page;
      })
      .catch(() => {})
      .finally(() => setRefreshing(false));
  }, []);

  // The refreshed first page replaces the rows once the spinner is gone, at the top of the list.
  const applyRefresh = useCallback(() => {
    const page = heldPageRef.current;
    heldPageRef.current = null;
    if (!page) return;
    generationRef.current += 1;
    takePage(page);
    const { created, hidden } = editsRef.current;
    listRef.current?.setData([
      ...created.filter((row) => !hidden.has(row.id)),
      ...rowsOf(page.items),
    ]);
    // MVCP keeps the old first post in place; the reader pulled for what is new, above it.
    listRef.current?.scrollToStart();
  }, [rowsOf, takePage]);

  useHeaderActions({
    onPrepend: () =>
      publishPosts(PUBLISH_COUNT).then(
        (created) => listRef.current?.prependItems(rowsOf(created)),
        () => {}
      ),
    onAppend: loadMore,
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
        edit(key, toggleLike(item));
      } else if (action === 'hide') {
        hide([key]);
      }
    },
    [edit, hide]
  );

  const prependLocal = useCallback(() => {
    const fresh = Array.from({ length: 3 }, () => {
      publishedRef.current += 1;
      return toRow(
        generateFeedElement(publishedRef.current),
        paletteRef.current
      );
    });
    editsRef.current.created.unshift(...fresh);
    listRef.current?.prependItems(fresh);
  }, []);

  const removeFirst = useCallback(() => {
    const first = listRef.current?.getKeys()[0];
    if (first !== undefined) hide([first]);
  }, [hide]);

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
    edit(key, {
      ...toggleLike(item),
      text: `${item.text.replace(/ \(edited\)$/, '')} (edited)`,
    });
  }, [edit]);

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
    /*
     * Liked and not liked are two elements styled by the template (theme colors), toggled by
     * data. U+FE0E keeps the heart a text glyph; Android draws a bare U+2665 as a color emoji.
     */
    const actions = (
      <View style={styles.actions}>
        <ShadowListNative.View action="like" style={styles.actionHit}>
          <ShadowListNative.Text
            style={styles.action}
            bind={{ text: '\u2661 {likes}', hidden: 'liked' }}
          />
          <ShadowListNative.Text
            style={[styles.action, styles.actionLiked]}
            bind={{ text: '\u2665\uFE0E {likes}', visible: 'liked' }}
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

  if (initialRows === null) {
    return <QueryStatus error={error} onRetry={loadFirstPage} />;
  }

  return (
    <View style={screenStyles.container}>
      <ShadowListNative
        ref={listRef}
        initialData={initialRows}
        templates={templates}
        templateKey="type"
        style={screenStyles.list}
        autoHideHeader
        refreshing={refreshing}
        onRefresh={handleRefresh}
        onRefreshSettle={applyRefresh}
        refreshColor={colors.secondaryLabel}
        onEndReached={loadMore}
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
    actionLiked: {
      color: theme.colors.accent,
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
