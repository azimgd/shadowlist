import { IMAGE_TITLES } from './common';

export interface StripItem {
  id: string;
  title: string;
  header: boolean;
  colorIndex: number;
}

export function buildStrip(): StripItem[] {
  const titles = Array.from({ length: 600 }, (_, index) => ({
    title: IMAGE_TITLES[index % IMAGE_TITLES.length]!,
    index,
  })).sort((a, b) => a.title.localeCompare(b.title));
  const items: StripItem[] = [];
  let letter = '';
  for (const { title, index } of titles) {
    const first = title.charAt(0).toUpperCase();
    if (first !== letter) {
      letter = first;
      items.push({
        id: `head-${first}`,
        title: first,
        header: true,
        colorIndex: 0,
      });
    }
    items.push({
      id: `strip-${index}`,
      title,
      header: false,
      colorIndex: items.length,
    });
  }
  return items;
}
