# React Flow Integration Boundary

This folder is reserved for third-party dependency assets used by BlueprintQueryAnalysis.

React Flow is a React/Web renderer and cannot be linked directly into an Unreal Slate widget. The current graph viewer is hosted by Unreal's WebBrowser widget and loads:

```text
Private/Dependencylibraries/ReactFlow/dist/index.html
```

The current `dist/index.html` is a self-contained offline frontend that implements the requested React Flow-style behavior without a network dependency. It is loaded through Unreal's WebBrowser from a local `file:///` URL, so it does not request a CDN or remote host:

- directed dependency edges
- white glowing animated flow segments
- connected Blueprint nodes with blue glow
- connected non-Blueprint asset nodes with teal glow
- disconnected nodes in grey
- folder drill-down from the `/All/Game` overview into per-folder asset dependency views
- runtime flow lanes with a horizontal branch layout

If the plugin later needs the actual `reactflow` npm package, replace the self-contained file with a bundled frontend output here, for example:

```text
Private/Dependencylibraries/ReactFlow/dist/
```

Do not load React Flow from a CDN at editor runtime. Bundle and version the built frontend assets with the plugin so the editor window works offline and deterministically.
