import {
  forwardRef,
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
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import {
  ArrowUpIcon,
  ChevronIcon,
  CloseIcon,
  MicIcon,
  PencilIcon,
  PlusIcon,
  StopIcon,
} from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import { keyboardAppearanceFor } from '../internal/keyboardAppearance';
import { AssistantActionButton } from './AssistantActionButton';
import { AssistantAttachmentChip } from './AssistantAttachmentChip';
import type { AssistantAttachment } from './types';

export interface AssistantComposerHandle {
  /*
   * Replaces the text and focuses the input, like when editing an earlier prompt. Pass
   * focus false to keep the keyboard down, like while dictating.
   */
  setDraft: (text: string, options?: { focus?: boolean }) => void;
  getDraft: () => string;
  // Clears the text without focusing, so the keyboard doesn't rise.
  clearDraft: () => void;
  focus: () => void;
}

export interface AssistantComposerProps {
  // The composer clears its own text. The caller owns attachments and clears those.
  onSend: (text: string, attachments: readonly AssistantAttachment[]) => void;
  // While a reply streams, Send becomes Stop.
  streaming: boolean;
  onStop?: () => void;
  /*
   * While a reply streams and there is text in the box, show Send rather than Stop: the caller
   * takes a message sent mid-reply as "stop that and do this instead". Without it, the only way
   * to redirect a reply that was going the wrong way was Stop, then retype, then Send -- and the
   * text typed while waiting sat in the box with nowhere to go.
   */
  sendWhileStreaming?: boolean;
  attachments?: readonly AssistantAttachment[];
  // Shows the add attachment button when given.
  onPressAttach?: () => void;
  onRemoveAttachment?: (attachmentId: string) => void;
  // Shows the model pill when given. Pressing it calls onPressModel to open a picker.
  model?: string;
  onPressModel?: () => void;
  // Shows the thinking toggle when onThinkingChange is given.
  thinking?: boolean;
  onThinkingChange?: (enabled: boolean) => void;
  // Shows the microphone button when onPressDictate is given. dictating marks it active.
  dictating?: boolean;
  onPressDictate?: () => void;
  // Shows the editing banner.
  editing?: boolean;
  onCancelEdit?: () => void;
  maxLength?: number;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

const NO_ATTACHMENTS: readonly AssistantAttachment[] = [];

export const AssistantComposer = forwardRef<
  AssistantComposerHandle,
  AssistantComposerProps
>(
  (
    {
      onSend,
      streaming,
      onStop,
      sendWhileStreaming = false,
      attachments = NO_ATTACHMENTS,
      onPressAttach,
      onRemoveAttachment,
      model,
      onPressModel,
      thinking = false,
      onThinkingChange,
      dictating = false,
      onPressDictate,
      editing = false,
      onCancelEdit,
      maxLength = 4000,
      labels,
      style,
    },
    ref
  ) => {
    const theme = useTheme();
    const { colors } = theme;
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    const insets = useSafeAreaInsets();
    const inputRef = useRef<ComponentRef<typeof TextInput>>(null);
    const [text, setText] = useState('');
    // Mirrors text so getDraft reads the latest value without a new handle.
    const textRef = useRef(text);
    textRef.current = text;

    useImperativeHandle(
      ref,
      () => ({
        setDraft: (nextText, options) => {
          setText(nextText);
          if (options?.focus !== false) inputRef.current?.focus();
        },
        getDraft: () => textRef.current,
        clearDraft: () => setText(''),
        focus: () => inputRef.current?.focus(),
      }),
      []
    );

    const hasDraft = text.trim().length > 0 || attachments.length > 0;
    const showStop = streaming && !(sendWhileStreaming && hasDraft);
    const canSend = hasDraft && (!streaming || sendWhileStreaming);

    const handleSend = () => {
      if (!canSend) return;
      onSend(text.trim(), attachments);
      setText('');
    };

    return (
      <View
        style={[
          styles.container,
          { paddingBottom: insets.bottom || theme.spacing.sm },
          style,
        ]}
      >
        {editing ? (
          <View style={styles.editBanner}>
            <PencilIcon size={14} color={colors.accent} strokeWidth={1.3} />
            <Text style={styles.editText}>{l.editingMessage}</Text>
            <Pressable
              onPress={onCancelEdit}
              hitSlop={12}
              accessibilityRole="button"
              accessibilityLabel={l.cancelEditing}
            >
              <CloseIcon
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
                  onRemove={onRemoveAttachment}
                  labels={l}
                />
              ))}
            </ScrollView>
          ) : null}

          <TextInput
            ref={inputRef}
            style={styles.input}
            value={text}
            onChangeText={setText}
            placeholder={l.placeholder}
            placeholderTextColor={colors.secondaryLabel}
            accessibilityLabel={l.placeholder}
            multiline
            maxLength={maxLength}
            keyboardAppearance={keyboardAppearanceFor(theme)}
          />

          <View style={styles.toolbar}>
            {onPressAttach ? (
              <AssistantActionButton
                label={l.addAttachment}
                onPress={onPressAttach}
              >
                <PlusIcon size={18} color={colors.label} />
              </AssistantActionButton>
            ) : null}

            {model !== undefined ? (
              <Pressable
                onPress={onPressModel}
                disabled={!onPressModel}
                hitSlop={6}
                accessibilityRole="button"
                accessibilityLabel={l.model(model)}
                accessibilityState={{ disabled: !onPressModel }}
                style={({ pressed }) => [
                  styles.pill,
                  pressed && styles.pressed,
                ]}
              >
                <Text style={styles.pillText}>{model}</Text>
                {onPressModel ? (
                  <ChevronIcon
                    direction="down"
                    size={10}
                    color={colors.secondaryLabel}
                    strokeWidth={1.8}
                  />
                ) : null}
              </Pressable>
            ) : null}

            {onThinkingChange ? (
              <Pressable
                onPress={() => onThinkingChange(!thinking)}
                hitSlop={6}
                accessibilityRole="switch"
                accessibilityLabel={l.thinkingToggleDescription}
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
                  {l.thinkingToggle}
                </Text>
              </Pressable>
            ) : null}

            <View style={styles.spacer} />

            {onPressDictate ? (
              <Pressable
                onPress={onPressDictate}
                hitSlop={6}
                accessibilityRole="button"
                accessibilityLabel={dictating ? l.stopDictating : l.dictate}
                accessibilityState={{ selected: dictating }}
                style={({ pressed }) => [
                  styles.sendButton,
                  styles.dictateButton,
                  dictating && styles.dictateButtonActive,
                  pressed && styles.pressed,
                ]}
              >
                <MicIcon
                  size={20}
                  color={dictating ? colors.accent : colors.label}
                />
              </Pressable>
            ) : null}

            {showStop ? (
              <Pressable
                onPress={onStop}
                disabled={!onStop}
                hitSlop={6}
                accessibilityRole="button"
                accessibilityLabel={l.stop}
                style={({ pressed }) => [
                  styles.sendButton,
                  styles.stopButton,
                  pressed && styles.pressed,
                ]}
              >
                <StopIcon size={22} color={colors.background} />
              </Pressable>
            ) : (
              <Pressable
                onPress={handleSend}
                disabled={!canSend}
                hitSlop={6}
                accessibilityRole="button"
                accessibilityLabel={l.send}
                accessibilityState={{ disabled: !canSend }}
                style={({ pressed }) => [
                  styles.sendButton,
                  !canSend && styles.sendButtonDisabled,
                  pressed && styles.pressed,
                ]}
              >
                <ArrowUpIcon
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

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      paddingHorizontal: theme.spacing.sm,
      paddingTop: theme.spacing.sm,
      backgroundColor: theme.colors.background,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: theme.colors.separator,
    },
    editBanner: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.sm,
      paddingHorizontal: theme.spacing.md,
      paddingBottom: theme.spacing.sm,
    },
    editText: {
      flex: 1,
      color: theme.colors.accent,
      ...theme.typography.footnote,
      fontWeight: theme.fontWeight.semibold,
    },
    field: {
      backgroundColor: theme.colors.elevated,
      borderRadius: theme.radius.xl,
      borderWidth: StyleSheet.hairlineWidth,
      borderColor: theme.colors.separator,
      paddingTop: theme.spacing.sm,
    },
    tray: {
      gap: theme.spacing.sm,
      paddingHorizontal: theme.spacing.sm,
      paddingBottom: theme.spacing.sm,
    },
    input: {
      color: theme.colors.label,
      ...theme.typography.body,
      minHeight: 36,
      maxHeight: 140,
      paddingHorizontal: theme.spacing.md,
      paddingTop: theme.spacing.xs,
      paddingBottom: theme.spacing.xs,
      margin: 0,
    },
    toolbar: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
      paddingHorizontal: theme.spacing.xs + 2,
      paddingBottom: theme.spacing.xs + 2,
    },
    pill: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
      height: 30,
      paddingHorizontal: theme.spacing.md,
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.fill,
    },
    pillActive: {
      backgroundColor: theme.colors.accentSoft,
    },
    pillText: {
      color: theme.colors.label,
      ...theme.typography.footnote,
      fontWeight: theme.fontWeight.semibold,
    },
    pillTextActive: {
      color: theme.colors.accent,
    },
    spacer: {
      flex: 1,
    },
    sendButton: {
      width: 32,
      height: 32,
      borderRadius: theme.radius.lg,
      backgroundColor: theme.colors.accent,
      alignItems: 'center',
      justifyContent: 'center',
    },
    sendButtonDisabled: {
      backgroundColor: theme.colors.fill,
    },
    dictateButton: {
      backgroundColor: 'transparent',
    },
    dictateButtonActive: {
      backgroundColor: theme.colors.accentSoft,
    },
    stopButton: {
      backgroundColor: theme.colors.label,
    },
    pressed: {
      opacity: 0.6,
    },
  })
);
