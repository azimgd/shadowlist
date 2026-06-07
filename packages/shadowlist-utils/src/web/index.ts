/*
 * Reusable React web (DOM) list templates for the `shadowlist-wasm`
 * virtualization library. Each domain is a dot-namespaced compound component —
 * e.g. <Feed.List data={posts} /> with <Feed.Element /> — ready to use and fully
 * overridable. Shared tokens, icons and primitives are exported alongside.
 *
 * Web entry point (`shadowlist-utils/web`). The package root
 * (`shadowlist-utils`) stays framework-agnostic and holds the data shapes +
 * sample-data generators these templates consume.
 */

// Design tokens + icon set.
export * from './theme';
export * from './icons';

// Shared, cross-domain primitives.
export { Spinner } from './primitives/Spinner';
export { ListHeader } from './primitives/ListHeader';
export type { ListHeaderProps } from './primitives/ListHeader';
export { ListFooter } from './primitives/ListFooter';
export type { ListFooterProps } from './primitives/ListFooter';
export { ItemSeparator } from './primitives/ItemSeparator';
export { SectionHeader } from './primitives/SectionHeader';
export type { SectionHeaderProps } from './primitives/SectionHeader';

// Domain compound components.
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
