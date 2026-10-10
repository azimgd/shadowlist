import { useRef, useState, useMemo, useCallback } from 'react';
import { View, StyleSheet } from 'react-native';
import { type TreeListCommands } from 'shadowlist';
import { collectExpandableKeys } from 'shadowlist-utils';
import {
  ListFooter,
  Tree,
  createStyles,
  type TreeNode,
} from 'shadowlist-utils/native';
import { useHeaderMenu } from './HeaderActions';
import { DEBUG } from '../launchSettings';
import { QueryStatus } from './QueryStatus';
import { useFileTreeQuery } from '../queries/files';

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

  const allFolderKeys = useMemo(
    () => collectExpandableKeys(tree, { getChildren, keyExtractor }),
    [tree]
  );

  const [expandedKeys, setExpandedKeys] = useState<Set<string>>(
    () => new Set(tree.map(keyExtractor))
  );

  const expandAll = useCallback(
    () => setExpandedKeys(new Set(allFolderKeys)),
    [allFolderKeys]
  );
  const collapseAll = useCallback(() => setExpandedKeys(new Set()), []);

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

  const openCount = expandedKeys.size;
  const folderCount = allFolderKeys.length;
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
        expandedKeys={expandedKeys}
        onExpandedChange={setExpandedKeys}
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
