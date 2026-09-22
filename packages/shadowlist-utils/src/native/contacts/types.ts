export interface ContactItem {
  id: string;
  name: string;
  subtitle?: string;
  avatarUrl?: string;
  // Defaults to a color derived from name.
  avatarColor?: string;
}
