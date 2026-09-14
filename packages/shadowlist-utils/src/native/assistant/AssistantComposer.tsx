import {
  forwardRef,
  useCallback,
  useImperativeHandle,
  useRef,
  useState,
  type ComponentRef,
} from 'react';
import {
  View,
  Text,
  TextInput,
  Pressable,
  ScrollView,
  StyleSheet,
} from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { colors, typography, spacing, radius, fontWeight } from '../theme';
import { ArrowUp, Chevron, Close, Pencil, Plus, Stop } from '../icons';
import { AssistantActionButton } from './AssistantActionButton';
import { AssistantAttachmentChip } from './AssistantAttachmentChip';
import { buildAttachment, type AssistantAttachment } from './data';

export interface AssistantComposerHandle {
  // Replace the draft and focus the input, e.g. to edit and resend a message.
  setDraft: (text: string, attachments?: AssistantAttachment[]) => void;
  /*
   * Empty the draft WITHOUT focusing. Cancelling an edit is the caller putting the composer
   * back as it was; focusing there raises the keyboard over the conversation the reader
   * just chose to go back to.
   */
  clearDraft: () => void;
  focus: () => void;
}

export interface AssistantComposerProps {
  onSend: (text: string, attachments: AssistantAttachment[]) => void;
  onStop: () => void;
  // A reply is streaming: the send button turns into stop.
  streaming: boolean;
  model: string;
  onCycleModel: () => void;
  // Extended thinking: replies reason before answering.
  thinking: boolean;
  onToggleThinking: () => void;
  // Set while an edited message is in the draft; shows a banner with a cancel.
  editing?: boolean;
  onCancelEdit?: () => void;
  placeholder?: string;
}

/*
 * Composer: attachment tray, growing multiline field, and a toolbar with attach, model
 * picker, thinking toggle and a send button that becomes stop while a reply streams. The
 * draft stays local, so typing re-renders only the composer, never the list.
 */
export const AssistantComposer = forwardRef<
  AssistantComposerHandle,
  AssistantComposerProps
>(
  (
    {
      onSend,
      onStop,
      streaming,
      model,
      onCycleModel,
      thinking,
      onToggleThinking,
      editing = false,
      onCancelEdit,
      placeholder = 'Ask anything',
    },
    ref
  ) => {
    const insets = useSafeAreaInsets();
    const inputRef = useRef<ComponentRef<typeof TextInput>>(null);
    const [text, setText] = useState('');
    const [attachments, setAttachments] = useState<AssistantAttachment[]>([]);
    // Cycles the demo attachment seeds on each tap of the attach button.
    const attachCount = useRef(0);

    useImperativeHandle(
      ref,
      () => ({
        setDraft: (nextText, nextAttachments = []) => {
          setText(nextText);
          setAttachments(nextAttachments);
          inputRef.current?.focus();
        },
        clearDraft: () => {
          setText('');
          setAttachments([]);
        },
        focus: () => inputRef.current?.focus(),
      }),
      []
    );

    const canSend =
      !streaming && (text.trim().length > 0 || attachments.length > 0);

    /*
     * Stable identities, because `handleRemove` is a prop of every memoized attachment chip
     * and the draft changes on every keystroke: a fresh closure per character would
     * re-render the whole tray while typing.
     */
    const handleSend = useCallback(() => {
      if (!canSend) return;
      onSend(text.trim(), attachments);
      setText('');
      setAttachments([]);
    }, [canSend, onSend, text, attachments]);

    const handleAttach = useCallback(() => {
      setAttachments((prev) => [
        ...prev,
        buildAttachment(attachCount.current++),
      ]);
    }, []);

    const handleRemove = useCallback(
      (attachmentId: string) =>
        setAttachments((prev) =>
          prev.filter((attachment) => attachment.id !== attachmentId)
        ),
      []
    );

    return (
      <View
        style={[
          styles.container,
          { paddingBottom: insets.bottom || spacing.sm },
        ]}
      >
        {editing ? (
          <View style={styles.editBanner}>
            <Pencil size={14} color={colors.accent} strokeWidth={1.3} />
            <Text style={styles.editText}>Editing message</Text>
            <Pressable
              onPress={onCancelEdit}
              hitSlop={12}
              accessibilityRole="button"
              accessibilityLabel="Cancel editing"
            >
              <Close
                size={16}
                color={colors.secondaryLabel}
                strokeWidth={1.8}
              />
            </Pressable>
          </View>
        ) : null}

        <View style={styles.field}>
          {attachments.length > 0 ? (
            <ScrollView
              horizontal
              showsHorizontalScrollIndicator={false}
              keyboardShouldPersistTaps="handled"
              contentContainerStyle={styles.tray}
            >
              {attachments.map((attachment) => (
                <AssistantAttachmentChip
                  key={attachment.id}
                  attachment={attachment}
                  onRemove={handleRemove}
                />
              ))}
            </ScrollView>
          ) : null}

          <TextInput
            ref={inputRef}
            style={styles.input}
            value={text}
            onChangeText={setText}
            placeholder={placeholder}
            placeholderTextColor={colors.secondaryLabel}
            multiline
            maxLength={4000}
            // The composer is dark; the default light keyboard flashes white against it.
            keyboardAppearance="dark"
          />

          <View style={styles.toolbar}>
            <AssistantActionButton
              label="Add attachment"
              onPress={handleAttach}
            >
              <Plus size={18} color={colors.label} />
            </AssistantActionButton>

            <Pressable
              onPress={onCycleModel}
              hitSlop={6}
              accessibilityRole="button"
              accessibilityLabel={`Model: ${model}`}
              style={({ pressed }) => [styles.pill, pressed && styles.pressed]}
            >
              <Text style={styles.pillText}>{model}</Text>
              <Chevron
                direction="down"
                size={10}
                color={colors.secondaryLabel}
                strokeWidth={1.8}
              />
            </Pressable>

            <Pressable
              onPress={onToggleThinking}
              hitSlop={6}
              accessibilityRole="switch"
              accessibilityLabel="Extended thinking"
              accessibilityState={{ checked: thinking }}
              style={({ pressed }) => [
                styles.pill,
                thinking && styles.pillActive,
                pressed && styles.pressed,
              ]}
            >
              <Text
                style={[styles.pillText, thinking && styles.pillTextActive]}
              >
                Think
              </Text>
            </Pressable>

            <View style={styles.spacer} />

            {streaming ? (
              <Pressable
                onPress={onStop}
                hitSlop={6}
                accessibilityRole="button"
                accessibilityLabel="Stop generating"
                style={({ pressed }) => [
                  styles.sendButton,
                  styles.stopButton,
                  pressed && styles.pressed,
                ]}
              >
                <Stop size={22} color={colors.background} />
              </Pressable>
            ) : (
              <Pressable
                onPress={handleSend}
                disabled={!canSend}
                hitSlop={6}
                accessibilityRole="button"
                accessibilityLabel="Send"
                accessibilityState={{ disabled: !canSend }}
                style={({ pressed }) => [
                  styles.sendButton,
                  !canSend && styles.sendButtonDisabled,
                  pressed && styles.pressed,
                ]}
              >
                <ArrowUp
                  size={18}
                  color={canSend ? colors.label : colors.secondaryLabel}
                  strokeWidth={2.4}
                />
              </Pressable>
            )}
          </View>
        </View>
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    paddingHorizontal: spacing.sm,
    paddingTop: spacing.sm,
    backgroundColor: colors.background,
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: colors.separator,
  },
  editBanner: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.sm,
    paddingHorizontal: spacing.md,
    paddingBottom: spacing.sm,
  },
  editText: {
    flex: 1,
    color: colors.accent,
    ...typography.footnote,
    fontWeight: fontWeight.semibold,
  },
  field: {
    backgroundColor: colors.elevated,
    borderRadius: radius.xl,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: colors.separator,
    paddingTop: spacing.sm,
  },
  tray: {
    gap: spacing.sm,
    paddingHorizontal: spacing.sm,
    paddingBottom: spacing.sm,
  },
  input: {
    color: colors.label,
    ...typography.body,
    minHeight: 36,
    maxHeight: 140,
    paddingHorizontal: spacing.md,
    paddingTop: spacing.xs,
    paddingBottom: spacing.xs,
    margin: 0,
  },
  toolbar: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.xs,
    paddingHorizontal: spacing.xs + 2,
    paddingBottom: spacing.xs + 2,
  },
  pill: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.xs,
    height: 30,
    paddingHorizontal: spacing.md,
    borderRadius: radius.pill,
    backgroundColor: colors.fill,
  },
  pillActive: {
    backgroundColor: colors.accentSoft,
  },
  pillText: {
    color: colors.label,
    ...typography.footnote,
    fontWeight: fontWeight.semibold,
  },
  pillTextActive: {
    color: colors.accent,
  },
  spacer: {
    flex: 1,
  },
  sendButton: {
    width: 32,
    height: 32,
    borderRadius: radius.lg,
    backgroundColor: colors.accent,
    alignItems: 'center',
    justifyContent: 'center',
  },
  sendButtonDisabled: {
    backgroundColor: colors.fill,
  },
  stopButton: {
    backgroundColor: colors.label,
  },
  pressed: {
    opacity: 0.6,
  },
});
