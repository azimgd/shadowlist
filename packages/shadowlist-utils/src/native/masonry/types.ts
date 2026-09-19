export interface MasonryImage {
  uri: string;
  // Natural size: the card keeps this aspect ratio at any column width.
  width: number;
  height: number;
  alt?: string;
}

export interface MasonryItem {
  id: string;
  image: MasonryImage;
  title?: string;
}
