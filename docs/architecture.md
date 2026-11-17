# Arquitetura inicial

Este projeto parte de um fork da Godot 4.7.2 voltado a um fluxo de desenvolvimento AI-first. O runtime, o editor 2D/3D, o Inspector, a animação e as ferramentas nativas permanecem como a base principal e devem receber o mínimo possível de alterações.

As funcionalidades específicas do workspace de IA serão concentradas em `editor/ai_workspace/`, funcionando como uma camada de integração do editor. Agentes externos serão conectados por adaptadores bem delimitados, sem incorporar um modelo de IA próprio nem exigir servidores MCP de terceiros. A camada deverá reutilizar as APIs reais da versão presente, respeitar Undo/Redo e manter a compatibilidade dos projetos com a Godot oficial sempre que viável.

## Integração nativa inicial

A etapa 03 adiciona `AIWorkspaceEditorPlugin` em `editor/ai_workspace/`. A classe herda de `EditorPlugin`, cria apenas controles GUI nativos (`CenterContainer`, `VBoxContainer` e `Label`) e expõe a tela principal por meio de `has_main_screen()` e `make_visible(bool)`.

O plugin é registrado pelo `EditorNode` com `add_editor_plugin()`. A própria implementação existente de `EditorMainScreen` reconhece plugins com `has_main_screen()`, cria o botão com `get_plugin_name()` e alterna a visibilidade das telas. Assim, 2D, 3D, Script, Game e Asset Store continuam usando o fluxo nativo, sem alteração no runtime ou em `EditorMainScreen`.

A interface inicial contém somente o título, a descrição e o estado `No agent connected`. Conversas, agentes, MCP, terminal, Inspector inteligente, animação e Asset Slots permanecem fora do escopo desta etapa.

## Interface estrutural do workspace

A etapa 04 mantém `AIWorkspaceEditorPlugin` como responsável apenas pelo ciclo de vida da tela principal e concentra o layout em `AIWorkspaceMainScreen`. A tela utiliza somente controles GUI nativos e estilos do tema do editor: um cabeçalho de estado, uma área de conversa rolável com compositor inativo e um painel lateral de contexto que pode ser recolhido.

Os controles de entrada e as regiões de mensagens e contexto são mantidos como pontos estruturais para evolução posterior, sem implementar conexão com agentes, execução de comandos ou protocolos externos. O painel contextual reserva espaço para cena, nó, recursos e operações sem duplicar o viewport ou o Inspector nativos.

## Serviço nativo de contexto

A etapa 05 adiciona `AIWorkspaceContextService`, um serviço somente leitura em `editor/ai_workspace/`. Ele consulta o nome do projeto, a cena editada, a seleção atual e um conjunto limitado de propriedades editoriais e recursos diretamente associados aos nós selecionados. Nós removidos são revalidados por `ObjectDB` antes da coleta.

As atualizações usam os sinais nativos `EditorNode::scene_changed`, `EditorNode::scene_closed` e `EditorSelection::selection_changed`; não há polling contínuo. `AIWorkspaceMainScreen` solicita um snapshot ao serviço e atualiza o painel contextual, mantendo as operações do agente vazias. O snapshot é composto por `Dictionary`/`Array` sem ponteiros e pode ser inspecionado como JSON somente leitura na própria interface.

## Command Router nativo

A etapa 06 adiciona `AIWorkspaceCommandRouter` como a única entrada para alterações estruturadas na cena. O router aceita um objeto JSON com `command` e `args`, resolve somente caminhos relativos à raiz editada e retorna respostas formadas por `Dictionary`, sem expor ponteiros. A primeira versão possui apenas `node.create`, `node.set_property` e `node.delete`, com listas explícitas de tipos de nós e propriedades permitidos.

Todas as alterações são registradas no histórico da cena pelo `EditorUndoRedoManager`. Criação e remoção preservam parent, índice e owner; alterações de propriedade guardam o valor anterior. O `SceneTreeEditor`, a seleção e o Inspector são atualizados pelos mecanismos nativos do editor. Uma área técnica recolhível em `AIWorkspaceMainScreen` permite testes manuais com JSON, sem habilitar o compositor principal nem introduzir comunicação externa.
