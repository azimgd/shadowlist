import { memo, useMemo, useRef } from 'react';
import { View, StyleSheet, type StyleProp, type ViewStyle } from 'react-native';
import { useLabels } from '../labels';
import { createStyles } from '../theme';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import { openUrl } from './openUrl';
import { PulsingDot } from './AssistantTypingIndicator';
import { AssistantMarkdownBlock } from './AssistantMarkdownBlock';
import { parseMarkdownFrom, type MarkdownParse } from './markdown';

export interface AssistantMarkdownProps {
  text: string;
  streaming?: boolean;
  onCopyCode?: (code: string) => void;
  onOpenLink?: (url: string) => void;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

export const AssistantMarkdown = memo(
  ({
    text,
    streaming = false,
    onCopyCode,
    onOpenLink = openUrl,
    labels,
    style,
  }: AssistantMarkdownProps) => {
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    /*
     * While a reply streams the text only grows. Each flush continues the last parse
     * instead of reading the whole reply again.
     */
    const parseRef = useRef<MarkdownParse | null>(null);
    const blocks = useMemo(() => {
      const parse = parseMarkdownFrom(parseRef.current, text);
      parseRef.current = parse;
      return parse.blocks;
    }, [text]);

    return (
      <View style={[styles.container, style]}>
        {blocks.map((block) => (
          <AssistantMarkdownBlock
            key={block.key}
            block={block}
            onCopyCode={onCopyCode}
            onOpenLink={onOpenLink}
            labels={l}
          />
        ))}
        {streaming ? <PulsingDot /> : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      gap: theme.spacing.md,
    },
  })
);
