import { defineConfig } from 'astro/config';

export default defineConfig({
  site: 'https://rosie.libs.technology',
  // Default static output; dist/ is what we publish via wrangler.
  markdown: {
    shikiConfig: {
      // Dark theme matching the page's monospace/terminal aesthetic.
      theme: 'github-dark-default',
    },
  },
});
