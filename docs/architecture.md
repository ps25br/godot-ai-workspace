# Arquitetura inicial

Este projeto parte de um fork da Godot 4.7.2 voltado a um fluxo de desenvolvimento AI-first. O runtime, o editor 2D/3D, o Inspector, a animação e as ferramentas nativas permanecem como a base principal e devem receber o mínimo possível de alterações.

As funcionalidades específicas do workspace de IA serão concentradas em `editor/ai_workspace/`, funcionando como uma camada de integração do editor. Agentes externos serão conectados por adaptadores bem delimitados, sem incorporar um modelo de IA próprio nem exigir servidores MCP de terceiros. A camada deverá reutilizar as APIs reais da versão presente, respeitar Undo/Redo e manter a compatibilidade dos projetos com a Godot oficial sempre que viável.

## Integração nativa inicial

A etapa 03 adiciona `AIWorkspaceEditorPlugin` em `editor/ai_workspace/`. A classe herda de `EditorPlugin`, cria apenas controles GUI nativos (`CenterContainer`, `VBoxContainer` e `Label`) e expõe a tela principal por meio de `has_main_screen()` e `make_visible(bool)`.

O plugin é registrado pelo `EditorNode` com `add_editor_plugin()`. A própria implementação existente de `EditorMainScreen` reconhece plugins com `has_main_screen()`, cria o botão com `get_plugin_name()` e alterna a visibilidade das telas. Assim, 2D, 3D, Script, Game e Asset Store continuam usando o fluxo nativo, sem alteração no runtime ou em `EditorMainScreen`.

A interface inicial contém somente o título, a descrição e o estado `No agent connected`. Conversas, agentes, MCP, terminal, Inspector inteligente, animação e Asset Slots permanecem fora do escopo desta etapa.
