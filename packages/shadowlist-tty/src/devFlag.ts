/*
 * Fabric's list code reads __DEV__, which React Native's runtime sets and Node has not. A bundle's
 * define replaces those reads and leaves this unused.
 */
const scope = globalThis as { __DEV__?: boolean };
scope.__DEV__ ??= process.env.NODE_ENV !== 'production';
