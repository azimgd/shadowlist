export interface SnapImage {
  uri: string;
  alt?: string;
}

export interface SnapItem {
  id: string;
  title?: string;
  subtitle?: string;
  image?: SnapImage;
  // Card background. Falls back to the theme's elevated color.
  color?: string;
}
