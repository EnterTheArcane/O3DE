import * as esbuild from "esbuild";

const shared = {
    bundle: true,
    platform: "node",
    target: "es2024",
    format: "esm",
    minify: true,
    legalComments: "none",
    banner: {
        js: 'import{createRequire}from"module";const require=createRequire(import.meta.url);',
    },
};

await esbuild.build({
    ...shared,
    entryPoints: ["src/main.ts"],
    outfile: "dist/main/index.js",
});
