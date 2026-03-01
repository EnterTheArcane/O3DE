import * as esbuild from "esbuild";

const shared = {
    bundle: true,
    platform: "node",
    target: "es2024",
    format: "esm",
    minify: true,
    legalComments: "none",
};

await Promise.all([
    esbuild.build({
        ...shared,
        entryPoints: ["src/main.ts"],
        outfile: "dist/main/index.js",
    }),
    
    esbuild.build({
        ...shared,
        entryPoints: ["src/post.ts"],
        outfile: "dist/post/index.js"
    }),
]);
