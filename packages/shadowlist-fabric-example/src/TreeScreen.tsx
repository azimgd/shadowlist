import { useRef, useState, useMemo, useCallback } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { type TreeListCommands } from 'shadowlist';
import { collectExpandableIds } from 'shadowlist-utils';
import {
  Tree,
  ListHeader,
  createStyles,
  type TreeNode,
} from 'shadowlist-utils/native';
import { QueryStatus } from './QueryStatus';
import { useFileTreeQuery } from './queries/files';

const getChildren = (node: TreeNode) => node.children;
const keyExtractor = (node: TreeNode) => node.id;

export const TreeScreen = () => {
  const files = useFileTreeQuery();

  if (files.data === undefined) {
    return <QueryStatus error={files.error} onRetry={files.refetch} />;
  }
  return <FileTree tree={files.data} />;
};

const HEADER = (
  <ListHeader
    title="Trip Files"
    subtitle="Passes, bookings and photos; collapse all for a short list"
  />
);

const FileTree = ({ tree }: { tree: TreeNode[] }) => {
  const treeRef = useRef<TreeListCommands>(null);
  const styles = useStyles();
  const insets = useSafeAreaInsets();

  const allFolderIds = useMemo(
    () => collectExpandableIds(tree, { getChildren, keyExtractor }),
    [tree]
  );

  const [expandedIds, setExpandedIds] = useState<Set<string>>(
    () => new Set(tree.map(keyExtractor))
  );

  const expandAll = useCallback(
    () => setExpandedIds(new Set(allFolderIds)),
    [allFolderIds]
  );
  const collapseAll = useCallback(() => setExpandedIds(new Set()), []);

  const openCount = expandedIds.size;
  const folderCount = allFolderIds.length;
  const footer = useMemo(
    () => (
      <View style={styles.statusFooter}>
        <Text style={styles.statusText}>
          {`${openCount} of ${folderCount} folders open`}
        </Text>
      </View>
    ),
    [styles, openCount, folderCount]
  );

  return (
    <View style={styles.container}>
      <Tree.List
        ref={treeRef}
        data={tree}
        expandedIds={expandedIds}
        onExpandedChange={setExpandedIds}
        style={styles.list}
        stickyHeader
        stickyFooter
        ListHeaderComponent={HEADER}
        ListFooterComponent={footer}
      />
      <View
        style={[styles.toolbar, { paddingBottom: (insets.bottom || 8) + 8 }]}
      >
        <Pressable
          accessibilityRole="button"
          style={({ pressed }) => [styles.button, pressed && styles.pressed]}
          onPress={expandAll}
        >
          <Text style={styles.buttonText}>Expand All</Text>
        </Pressable>
        <Pressable
          accessibilityRole="button"
          style={({ pressed }) => [styles.button, pressed && styles.pressed]}
          onPress={collapseAll}
        >
          <Text style={styles.buttonText}>Collapse All</Text>
        </Pressable>
      </View>
    </View>
  );
};

const useStyles = createStyles(({ colors, typography }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      backgroundColor: colors.background,
    },
    list: {
      flex: 1,
      backgroundColor: colors.background,
    },
    statusFooter: {
      width: '100%',
      alignItems: 'center',
      paddingVertical: 10,
      backgroundColor: colors.background,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
    },
    statusText: {
      color: colors.secondaryLabel,
      ...typography.footnote,
    },
    toolbar: {
      flexDirection: 'row',
      justifyContent: 'space-around',
      alignItems: 'center',
      paddingTop: 10,
      paddingHorizontal: 16,
      backgroundColor: colors.elevated,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
    },
    button: {
      paddingVertical: 6,
      paddingHorizontal: 12,
    },
    pressed: {
      opacity: 0.4,
    },
    buttonText: {
      color: colors.accent,
      ...typography.body,
    },
  })
);
