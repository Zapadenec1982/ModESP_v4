/**
 * Deploy script: gzip bundles and copy to data/www/
 * Usage: npm run deploy (builds + deploys)
 */
import { execSync } from 'child_process';
import { copyFileSync, existsSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const dist = join(__dirname, '..', 'dist');
const www = join(__dirname, '..', '..', 'data', 'www');

// Gzip bundles
console.log('Compressing bundles...');
for (const f of ['bundle.js', 'bundle.css']) {
  const src = join(dist, f);
  if (!existsSync(src)) {
    console.error(`Missing: ${src}`);
    process.exit(1);
  }
  execSync(`gzip -kf "${src}"`);
}

// Copy to data/www/
console.log(`Deploying to ${www}`);
copyFileSync(join(dist, 'index.html'), join(www, 'index.html'));
copyFileSync(join(dist, 'bundle.js.gz'), join(www, 'bundle.js.gz'));
copyFileSync(join(dist, 'bundle.css.gz'), join(www, 'bundle.css.gz'));

console.log('Done! Files deployed to data/www/');
