/*
 * Reusable React Native list templates for the `shadowlist` virtualization
 * library. Each domain is a dot-namespaced compound component — e.g.
 * `<Feed.List data={posts} />` with `<Feed.Element />` — that works out of the
 * box and is fully overridable. Shared design tokens, icons and primitives are
 * exported alongside so you can match and extend the look.
 *
 * This is the React Native entry point (`shadowlist-utils/native`). The package
 * root (`shadowlist-utils`) stays framework-agnostic and holds the data shapes +
 * sample-data generators these templates consume.
 */

export * from './theme';
export * from './icons';

export { Spinner } from './primitives/Spinner';
export { ListHeader } from './primitives/ListHeader';
export type { ListHeaderProps } from './primitives/ListHeader';
export { ListFooter } from './primitives/ListFooter';
export type { ListFooterProps } from './primitives/ListFooter';
export { ItemSeparator } from './primitives/ItemSeparator';
export { SectionHeader } from './primitives/SectionHeader';
export type { SectionHeaderProps } from './primitives/SectionHeader';

export * from './feed';
export * from './chat';
export * from './activity';
export * from './nested';
export * from './masonry';
export * from './contacts';
export * from './section';
export * from './reorder';
export * from './tree';
export * from './poll';
