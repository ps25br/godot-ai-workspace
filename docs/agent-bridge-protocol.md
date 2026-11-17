/**************************************************************************/
/*  agent-bridge-protocol.md                                              */
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

# Agent Bridge local

O Agent Bridge usa `TCPServer`/`StreamPeerTCP` nativos da Godot, limitado a `127.0.0.1:47831`. O transporte foi escolhido por ser multiplataforma, já estar disponível no núcleo e permitir comunicação entre o editor e um cliente Python sem dependências adicionais. Stdio não permite múltiplos clientes nem descoberta simples do editor; pipes nomeados seriam específicos de plataforma.

O Bridge fica desativado por padrão. A ativação normal é pelo botão **Enable Local Bridge** no AI Workspace. Para testes automatizados, a ativação explícita também pode ser feita ao iniciar o editor com `--ai-workspace-bridge --ai-workspace-bridge-secret=SEGREDO`.

## Mensagens

O protocolo usa uma mensagem JSON por linha:

```json
{
  "protocol": "ai-workspace/1",
  "id": "request-001",
  "auth": "session-secret",
  "method": "bridge.ping",
  "params": {}
}
```

Respostas têm o mesmo `protocol` e `id`, com `success` e `result`; falhas usam `success: false` e `error.code`/`error.message`.

## Métodos e aprovação

- `bridge.ping`: confirma que o Bridge está disponível.
- `context.get`: retorna o snapshot somente leitura do `AIWorkspaceContextService`.
- `command.execute`: encaminha um comando já existente ao `AIWorkspaceCommandRouter`.

`command.execute` nunca é executado somente por conexão ou autenticação. Ele fica pendente até o usuário pressionar **Approve Command**. **Reject Command** responde `COMMAND_REJECTED`. O comando aprovado usa o mesmo Undo/Redo nativo do Router.

## Limites e segurança

- Apenas loopback IPv4 (`127.0.0.1`); nenhuma porta pública.
- Uma conexão ativa por vez.
- Segredo aleatório de 32 bytes por sessão, exibido somente enquanto o Bridge está habilitado.
- Mensagens com até 64 KiB, respostas com até 256 KiB, profundidade JSON máxima 16 e no máximo 256 itens por estrutura.
- Sem shell, execução de processo, acesso externo a arquivos ou novos comandos de edição.
- Mensagens inválidas, protocolo desconhecido, método desconhecido e segredo inválido são rejeitados sem acessar a cena.

O Bridge e todas as chamadas ao Context Service/Command Router são processados na thread principal do editor. Ao desligar ou fechar o editor, a conexão e solicitações pendentes são encerradas; não há callbacks de thread secundária.

## Cliente de teste

`tools/ai_workspace_bridge_client.py` usa somente a biblioteca padrão do Python:

```powershell
python tools/ai_workspace_bridge_client.py --secret SEGREDO ping
python tools/ai_workspace_bridge_client.py --secret SEGREDO context
python tools/ai_workspace_bridge_client.py --secret SEGREDO self-test
python tools/ai_workspace_bridge_client.py --secret SEGREDO command node.create --args-json '{"parent_path":".","type":"Node2D","name":"Enemy"}'
```

O último comando permanece aguardando aprovação na UI. `self-test` verifica ping, contexto, método desconhecido, protocolo inválido, JSON inválido e mensagem excessiva.

Esta etapa não implementa agentes, Codex, OpenCode, Claude Code, MCP ou chat.
