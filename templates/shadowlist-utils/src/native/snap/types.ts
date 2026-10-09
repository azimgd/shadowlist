export interface SnapImage {
  uri: string;
  alt?: string;
}

export interface SnapItem {
  id: string;
  title?: string;
  subtitle?: string;
  image?: SnapImage;
  color?: string;
}
