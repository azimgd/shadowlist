import { memo } from 'react';
import {
  View,
  Text,
  Image,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { CloseIcon, DocIcon } from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import type { AssistantAttachment } from './types';

export interface AssistantAttachmentChipProps {
  attachment: AssistantAttachment;
  // Shows a remove button when given.
  onRemove?: (attachmentId: string) => void;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

export const AssistantAttachmentChip = memo(
  ({ attachment, onRemove, labels, style }: AssistantAttachmentChipProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    const isImage = attachment.kind === 'image';

    return (
      <View style={[styles.chip, style]}>
        <View
          style={[
            styles.thumb,
            isImage
              ? {
                  backgroundColor: attachment.color ?? theme.colors.elevated2,
                }
              : styles.thumbFile,
          ]}
        >
          {isImage ? (
            attachment.uri ? (
              <Image source={{ uri: attachment.uri }} style={styles.image} />
            ) : null
          ) : (
            <DocIcon
              size={18}
              color={attachment.color ?? theme.colors.secondaryLabel}
            />
          )}
        </View>
        <View style={styles.meta}>
          <Text style={styles.name} numberOfLines={1}>
            {attachment.name}
          </Text>
          {attachment.detail ? (
            <Text style={styles.detail}>{attachment.detail}</Text>
          ) : null}
        </View>
        {onRemove ? (
          <Pressable
            onPress={() => onRemove(attachment.id)}
            hitSlop={12}
            accessibilityRole="button"
            accessibilityLabel={l.removeAttachment(attachment.name)}
            style={({ pressed }) => [styles.remove, pressed && styles.pressed]}
          >
            <CloseIcon size={12} color={theme.colors.label} strokeWidth={1.8} />
          </Pressable>
        ) : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    chip: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.sm,
      maxWidth: 220,
      padding: theme.spacing.xs + 2,
      paddingRight: theme.spacing.md,
      borderRadius: theme.radius.md,
      backgroundColor: theme.colors.elevated,
    },
    thumb: {
      width: 36,
      height: 36,
      borderRadius: theme.radius.sm,
      alignItems: 'center',
      justifyContent: 'center',
      overflow: 'hidden',
    },
    thumbFile: {
      backgroundColor: theme.colors.elevated2,
    },
    image: {
      width: '100%',
      height: '100%',
    },
    meta: {
      flexShrink: 1,
    },
    name: {
      color: theme.colors.label,
      ...theme.typography.footnote,
      fontWeight: theme.fontWeight.semibold,
    },
    detail: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.caption,
    },
    remove: {
      width: 20,
      height: 20,
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.fill,
      alignItems: 'center',
      justifyContent: 'center',
      marginLeft: theme.spacing.xs,
    },
    pressed: {
      opacity: 0.35,
    },
  })
);
