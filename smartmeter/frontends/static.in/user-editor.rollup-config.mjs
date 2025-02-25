import pluginNodeResolve from "@rollup/plugin-node-resolve";

export default
{ input: "./static.in/user-editor.mjs"
, plugins: [ pluginNodeResolve() ]
, output: { file: "./static/user-editor.js", format: "es" }
}
