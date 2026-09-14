import { memo } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { colors, typography, spacing, radius, fontWeight } from '../theme';
import { Close, Doc } from '../icons';
import type { AssistantAttachment } from './data';

export interface AssistantAttachmentChipProps {
  attachment: AssistantAttachment;
  // When provided, a remove button calls this with the attachment id (composer tray).
  onRemove?: (attachmentId: string) => void;
}

// Thumbnail + name + size. Images show as a tinted tile, files as a document glyph.
export const AssistantAttachmentChip = memo(
  ({ attachment, onRemove }: AssistantAttachmentChipProps) => {
    const isImage = attachment.kind === 'image';
    return (
      <View style={styles.chip}>
        <View
          style={[
            styles.thumb,
            isImage ? { backgroundColor: attachment.color } : styles.thumbFile,
          ]}
        >
          {isImage ? null : <Doc size={18} color={attachment.color} />}
        </View>
        <View style={styles.meta}>
          <Text style={styles.name} numberOfLines={1}>
            {attachment.name}
          </Text>
          <Text style={styles.detail}>{attachment.detail}</Text>
        </View>
        {onRemove ? (
          <Pressable
            onPress={() => onRemove(attachment.id)}
            // A 12pt target inside a chip: the slop is what makes it reachable.
            hitSlop={12}
            accessibilityRole="button"
            accessibilityLabel={`Remove ${attachment.name}`}
            style={({ pressed }) => [styles.remove, pressed && styles.pressed]}
          >
            <Close size={12} color={colors.label} strokeWidth={1.8} />
          </Pressable>
        ) : null}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  chip: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.sm,
    maxWidth: 220,
    padding: spacing.xs + 2,
    paddingRight: spacing.md,
    borderRadius: radius.md,
    backgroundColor: colors.elevated,
  },
  thumb: {
    width: 36,
    height: 36,
    borderRadius: radius.sm,
    alignItems: 'center',
    justifyContent: 'center',
  },
  thumbFile: {
    backgroundColor: colors.elevated2,
  },
  meta: {
    flexShrink: 1,
  },
  name: {
    color: colors.label,
    ...typography.footnote,
    fontWeight: fontWeight.semibold,
  },
  detail: {
    color: colors.secondaryLabel,
    ...typography.caption,
  },
  remove: {
    width: 20,
    height: 20,
    borderRadius: radius.pill,
    backgroundColor: colors.fill,
    alignItems: 'center',
    justifyContent: 'center',
    marginLeft: spacing.xs,
  },
  pressed: {
    opacity: 0.35,
  },
});
