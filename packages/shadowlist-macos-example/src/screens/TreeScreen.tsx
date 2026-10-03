import { useRef, useState, useMemo, useCallback } from 'react';
import { View, StyleSheet } from 'react-native';
import { type TreeListCommands } from 'shadowlist';
import { collectExpandableIds } from 'shadowlist-utils';
import {
  ListFooter,
  Tree,
  createStyles,
  type TreeNode,
} from 'shadowlist-utils/native';
import { useHeaderMenu } from './HeaderActions';
import { DEBUG } from '@example/launchSettings';
import { QueryStatus } from '@example/screens/QueryStatus';
import { useFileTreeQuery } from '@example/queries/files';

const getChildren = (node: TreeNode) => node.children;
const keyExtractor = (node: TreeNode) => node.id;

export const TreeScreen = () => {
  const files = useFileTreeQuery();

  if (files.data === undefined) {
    return <QueryStatus error={files.error} onRetry={files.refetch} />;
  }
  return <FileTree tree={files.data} />;
};

const FileTree = ({ tree }: { tree: TreeNode[] }) => {
  const treeRef = useRef<TreeListCommands>(null);
  const styles = useStyles();

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

  useHeaderMenu([
    [
      {
        label: 'Expand All',
        symbol: 'chevron.down.2',
        onPress: expandAll,
      },
      {
        label: 'Collapse All',
        symbol: 'chevron.up.2',
        onPress: collapseAll,
      },
    ],
  ]);

  const openCount = expandedIds.size;
  const folderCount = allFolderIds.length;
  const footer = useMemo(
    () =>
      DEBUG ? (
        <ListFooter
          text={`${openCount} of ${folderCount} folders open`}
          style={styles.statusFooter}
        />
      ) : null,
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
        stickyFooter={DEBUG}
        ListFooterComponent={footer}
      />
    </View>
  );
};

const useStyles = createStyles(({ colors }) =>
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
      paddingVertical: 10,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
    },
  })
);
