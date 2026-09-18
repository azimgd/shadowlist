import type { TreeNode } from 'shadowlist-utils/native';
import { generateUniqueId } from './common';

export const FOLDER_NAMES = [
  'trips',
  'boarding-passes',
  'flights',
  'hotels',
  'photos',
  'visas',
  'receipts',
  'lisbon',
  'tokyo',
  'maps',
  'packing-lists',
  'notes',
];

export const FILE_EXTENSIONS = [
  'pdf',
  'ics',
  'jpg',
  'json',
  'txt',
  'gpx',
  'png',
  'heic',
];

/* Build a deep, wide Skyfy trip-files tree for the Tree list template. */
export function generateFileTree(
  rootCount = 4,
  maxDepth = 3,
  foldersPerLevel = 2,
  filesPerLevel = 4
): TreeNode[] {
  const buildFolder = (name: string, depth: number): TreeNode => {
    const children: TreeNode[] = [];

    if (depth < maxDepth) {
      for (let i = 0; i < foldersPerLevel; i++) {
        const childName = FOLDER_NAMES[(depth * 3 + i) % FOLDER_NAMES.length]!;
        children.push(buildFolder(childName, depth + 1));
      }
    }

    for (let i = 0; i < filesPerLevel; i++) {
      const ext = FILE_EXTENSIONS[(depth + i) % FILE_EXTENSIONS.length]!;
      const base = FOLDER_NAMES[(depth + i) % FOLDER_NAMES.length]!;
      children.push({
        id: generateUniqueId(),
        name: `${base}-${i}.${ext}`,
        kind: 'file',
      });
    }

    return {
      id: generateUniqueId(),
      name,
      kind: 'folder',
      children,
    };
  };

  return Array.from({ length: rootCount }, (_, index) =>
    buildFolder(FOLDER_NAMES[index % FOLDER_NAMES.length]!, 0)
  );
}
