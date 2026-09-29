export interface ReorderTileItem {
  id: string;
  title: string;
  // Large text on the swatch, like a number.
  label?: string;
  // Defaults to a color from theme.colors.avatarPalette picked by label or title.
  color?: string;
  // Swatch width over height. Mixed ratios give masonry columns.
  aspectRatio?: number;
}
