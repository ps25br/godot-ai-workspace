/**************************************************************************/
/*  architecture.md                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

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

## Operações nativas de cenas e recursos

A etapa 07 amplia o mesmo `AIWorkspaceCommandRouter` com `scene.create`, `scene.open`, `scene.save`, `resource.create` e `resource.assign`. Cenas novas são empacotadas por `PackedScene::pack()` e gravadas por `ResourceSaver`; abertura usa `EditorNode::open_scene()` e salvamento usa `EditorNode::save_scene_if_open()` somente quando a cena atual já possui caminho.

Os tipos de raiz permitidos são `Node2D` e `Node3D`. Recursos criáveis são limitados a `Gradient` e `GradientTexture2D`, gravados como `.tres`. A associação inicial é somente `Sprite2D.texture` com um recurso compatível `Texture2D`. Caminhos precisam ser normalizados, começar por `res://`, usar extensões permitidas, não conter `..`, protocolos adicionais, separadores alternativos ou componentes simbólicos.

Criação de arquivos não sobrescreve destinos existentes e não implementa exclusão. `resource.assign` usa `EditorUndoRedoManager` para restaurar o recurso anterior; abertura é navegação e bloqueia histórico não salvo; salvamento em disco não promete Undo/Redo. Nenhum recurso externo é alterado durante uma associação.
### Validação da etapa 07

O commit `db712c98765` foi compilado remotamente e validado com um projeto temporário. Foram testadas criação de cenas `Node2D`/`Node3D`, criação de `Gradient`/`GradientTexture2D`, associação de textura em `Sprite2D`, proteção contra alterações não salvas, abertura e salvamento de cenas, rejeição de sobrescrita e caminhos inválidos, incompatibilidade de recursos, Undo/Redo e persistência após reabertura do editor.
## Agent Bridge local

A etapa 08 adiciona `AIWorkspaceAgentBridge` em `editor/ai_workspace/`. Ele usa as APIs nativas `TCPServer` e `StreamPeerTCP`, faz bind somente em `127.0.0.1:47831` e processa JSON delimitado por linha na thread principal do editor. O Bridge começa desativado, gera um segredo de sessão por ativação e reutiliza `AIWorkspaceContextService` e `AIWorkspaceCommandRouter` sem duplicar suas regras.

`bridge.ping` e `context.get` são somente leitura. `command.execute` cria uma solicitação pendente e só é encaminhado ao Router após aprovação explícita na interface. O transporte não executa shell, não cria novos comandos e não altera o runtime.
