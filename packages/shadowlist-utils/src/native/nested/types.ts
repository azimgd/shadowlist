export interface NestedCardImage {
  uri: string;
  alt?: string;
}

export interface NestedCardItem {
  id: string;
  title: string;
  image: NestedCardImage;
}

export interface NestedItem {
  id: string;
  title: string;
  cards: ReadonlyArray<NestedCardItem>;
}
