import { defineConfig } from 'vite';
import viteCompression from 'vite-plugin-compression';

export default defineConfig({
    // Base path for your assets. '.' is good for relative paths when served from root
    base: './',
    build: {
        outDir: '../frontend_build',
        emptyOutDir: true, // Clean the output directory before building
        minify: 'terser', // 'terser' or 'esbuild' for JS minification
        cssMinify: true, // Enable CSS minification
    },
    plugins: [
        viteCompression({
            algorithm: 'gzip',
            ext: '.gz', // Add .gz extension to compressed files
            deleteOriginFile: true, // Keep the original uncompressed file if you want
            // threshold: 1024, // Only compress files larger than 1KB (adjust as needed)
            // filter: /\.(js|css|html|svg|json|xml|wasm)$/, // Only compress these types
            level: 9, // 1-9 (default: 6)
        })
    ],
    // For more specific optimizations (e.g., handling asset paths relative to ESP32)
    // publicDir: 'public', // If you have static assets not processed by Vite
});