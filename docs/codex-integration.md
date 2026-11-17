# Integração Codex

## Mecanismo escolhido

O AI Workspace usa o Codex App Server oficial através de um processo filho local iniciado com:

```text
codex app-server --listen stdio://
```

A comunicação é JSON-RPC delimitado por linhas. O ciclo mínimo implementado é `initialize`, `initialized`, `thread/start`, `turn/start`, `item/agentMessage/delta` e `turn/completed`. A aplicação não lê, salva ou exibe tokens. A autenticação permanece sob responsabilidade da instalação local do Codex; erros de credenciais são mostrados apenas como `Authentication required`.

Referências oficiais: [Codex app-server](https://developers.openai.com/siwc/token-sharing-open-source/codex-app-server), [Codex as a platform](https://developers.openai.com/blog/codex-as-a-platform) e [protocolo no repositório oficial](https://github.com/openai/codex/tree/main/codex-rs/app-server).

## Arquitetura e segurança

`AIWorkspaceCodexAdapter` fica em `editor/ai_workspace/agents/codex/`. Ele usa `OS::execute_with_pipe()` com argumentos constantes, monitora stdout sem shell genérico e encerra o processo no destrutor. A detecção usa somente `codex --version` e não registra sua saída.

Cada instrução recebe o JSON atual do `AIWorkspaceContextService` como contexto de leitura. O processo Codex não recebe ponteiros, acesso direto a objetos da Godot, comandos de shell do editor ou permissões automáticas.

O app-server oficial pode expor ferramentas de uma aplicação hospedeira, mas esta etapa não cria um servidor MCP nem uma camada adicional de ferramentas. Como ponte limitada, o adaptador reconhece uma linha explícita `AI_WORKSPACE_COMMAND:` contendo um objeto JSON. Essa convenção é local, não é apresentada como protocolo oficial: a proposta só é executada pelo `AIWorkspaceCommandRouter` depois de **Approve Command**; rejeição e ausência de aprovação não alteram a cena. O Router continua responsável por Undo/Redo.

## Fluxo

## Windows e instalação npm

No Windows, uma instalação npm normalmente expõe `codex.cmd`, que funciona no PowerShell mas não é um executável nativo para `CreateProcess`. O adapter resolve o launcher no `PATH` e, quando necessário, executa diretamente o `node.exe` com `node_modules/@openai/codex/bin/codex.js`. Assim a Godot não depende de shell genérico e continua usando o transporte `stdio://`.

1. Abra **AI Workspace** e pressione **Connect Codex**.
2. Confirme o estado exibido: `Disconnected`, `Not installed`, `Authentication required`, `Connecting`, `Connected` ou `Error`.
3. Envie uma instrução pelo campo existente. A resposta final aparece no painel de resposta.
4. Se houver proposta estruturada, use **Approve Command** ou **Reject Command**.
5. Pressione **Disconnect Codex**; o destrutor também encerra o processo filho.

## Limitações

## Chat, histórico e retomada

O ciclo do adaptador também usa `thread/resume` e `turn/interrupt`. O texto é exibido progressivamente a partir de `item/agentMessage/delta`; `Stop` preserva o texto recebido e marca a mensagem como interrompida. **New Chat** inicia uma thread nova. O adaptador salva somente `thread_id` e um histórico local limitado a 64 mensagens e 32768 caracteres por mensagem, usando metadados do projeto via `EditorSettings`; tokens e credenciais nunca são persistidos.

Uma thread persistida é retomada ao conectar. Se a thread não existir mais, o adaptador cria uma nova. A listagem de threads remotas não é exposta nesta etapa, pois o fluxo não busca o histórico remoto completo para preencher a interface sem misturar conversas.

Não há listagem/troca de múltiplas threads remotas, renovação OAuth, login no navegador, ferramentas oficiais expostas ao Codex ou suporte a OpenCode/Claude/MCP. A ponte de propostas é limitada e não substitui uma ferramenta oficial do app-server.

## Validação

### Etapa 10.1

O probe assíncrono `tools/ai_workspace_codex_probe.py` confirmou primeiro delta em 35,6 s, conclusão `completed` e retomada em 37,0 s. Com interrupção após o primeiro delta, confirmou `turn/completed=interrupted` e resposta de `turn/interrupt`. Esses resultados validam o protocolo real, mas não substituem a validação visual da tela Godot.

### Etapa 10

O código do commit `4d2fde81c47` foi compilado no workflow [44085486259](https://github.com/ps25br/godot-ai-workspace/actions/runs/44085486259). O executável confirmou `4.7.2.stable.gh.4d2fde81c`; o teste headless com projeto temporário terminou com código 0. O CLI real respondeu a `initialize`, `thread/start` e `thread/resume`. A sondagem de turno não concluiu dentro do timeout e a GUI não permaneceu aberta para screenshot, logo streaming visual, Stop e retomada pela interface ainda exigem validação em uma sessão gráfica disponível.

O ambiente local possui `codex-cli 0.163.0`; `codex login status` retornou sucesso sem que credenciais fossem lidas ou exibidas. O handshake e uma inferência real pelo App Server produziram deltas e `turn/completed` com status `completed`.

No artifact do commit `400b3c92a15`, a interface foi validada graficamente com o projeto temporário `stage07-test-project`. A instrução de leitura descreveu a cena real (`res://main.tscn`, raiz `Main`, seleção `Sprite (Sprite2D)`). A solicitação de criação gerou a proposta real `node.create`; **Approve Codex Command** criou `CodexEnemy`, Undo removeu o nó e Redo o restaurou. Uma sessão separada usando **Reject Codex Command** não criou o nó.

Evidências fora do repositório:

- `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage09-ai-workspace-response.png`
- `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage09-1-final-pending.png`
- `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage09-1-final-approved.png`
- `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage09-1-final-undo.png`
- `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage09-1-final-redo.png`
- `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage09-1-final-rejected.png`

Não há compilador C++ local, portanto a compilação do fork continua no workflow Windows do GitHub Actions. A validação gráfica usa um projeto temporário e não altera projetos pessoais.
