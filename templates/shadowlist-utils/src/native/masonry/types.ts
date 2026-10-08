export interface MasonryImage {
  uri: string;
  width: number;
  height: number;
  alt?: string;
}

export interface MasonryItem {
  id: string;
  image: MasonryImage;
  title?: string;
}
