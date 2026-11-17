/**************************************************************************/
/*  build.md                                                              */
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

# Build e manutenção do fork

## Estado inicial

Este diretório contém o código-fonte oficial da Godot `4.7.2-stable`.

- Branch de desenvolvimento: `ai-workspace`
- Remote oficial: `upstream` — `https://github.com/godotengine/godot.git`
- Commit-base verificado: `ed1daf0bf001b61586d9930840f2f1394092c079`

Não há `origin` configurado neste ambiente. Um remote de publicação pode ser adicionado posteriormente somente com autorização explícita.

## Dependências de compilação

Os requisitos abaixo foram conferidos no `SConstruct` e em `platform/windows/detect.py` desta versão:

- Windows 10/11 64-bit.
- Python 3.9 ou posterior. O ambiente atual possui Python 3.13.15.
- SCons 4.4 ou posterior. O ambiente atual não possui SCons.
- Visual Studio 2019 16.11 ou posterior, com compilador C++ MSVC e Windows SDK. O ambiente atual não expõe `cl.exe`, `msbuild` ou `vswhere` no `PATH`, e não foi detectada uma instalação por `vswhere.exe`.
- Git, necessário para obter e atualizar o código. O ambiente atual possui Git 2.55.0.

LLVM/Clang, MinGW e CMake não são necessários para o caminho padrão MSVC. Podem ser usados somente quando uma configuração de build específica exigir isso.

Nenhuma dependência foi instalada durante esta etapa. Quando autorizado, o SCons pode ser instalado em um ambiente Python apropriado, por exemplo:

```powershell
python -m pip install "scons>=4.4"
```

## Compilação no Windows

Abra um terminal com as ferramentas do Visual Studio disponíveis e execute na raiz do repositório:

```powershell
scons platform=windows target=editor -j2
```

O executável do editor será produzido na pasta `bin/`. A compilação ainda não foi executada neste ambiente porque faltam SCons e um compilador C++ detectável.

## Atualização futura a partir da Godot oficial

Antes de atualizar, verifique o estado local e preserve trabalho não commitado:

```powershell
git status
git switch ai-workspace
git fetch upstream --tags
```

Para avançar para uma nova tag estável, substitua o valor abaixo pela tag desejada e confirme o diff antes de continuar:

```powershell
$version = "4.7.3-stable"
git show "refs/tags/$version" --no-patch --decorate
git merge --ff-only "refs/tags/$version"
```

`--ff-only` impede que uma atualização crie um merge automático ou descarte alterações locais. Se houver trabalho específico do workspace, interrompa, revise os conflitos e atualize somente com autorização. Depois da atualização, execute a validação e a compilação disponíveis antes de declarar a etapa concluída.

Para conferir a origem e a versão atual:

```powershell
git remote -v
git branch --show-current
git describe --tags --abbrev=0 HEAD
git rev-parse HEAD
```

## Compilação remota com GitHub Actions

O workflow independente `.github/workflows/ai-workspace-windows.yml` é acionado somente por `workflow_dispatch`. Ele usa o runner `windows-latest`, MSVC, Python x64, SCons 4.10.1 e as ações locais reutilizáveis da Godot para dependências, compilação e cache.

A configuração produz somente o editor Windows `x86_64`, com `debug_symbols=no`, `lto=none` e sem templates de exportação. Os componentes opcionais D3D12, ANGLE, AccessKit e WinRT são desativados nesta primeira versão para evitar downloads e dependências adicionais; Vulkan, OpenGL, editor 2D/3D, GDScript, Inspector, animação, física e os módulos padrão permanecem habilitados.

### Iniciar uma compilação manual

1. Publique a branch `ai-workspace` em um repositório GitHub próprio, depois abra a aba **Actions**.
2. Selecione **AI Workspace - Windows Build**.
3. Clique em **Run workflow**, escolha `ai-workspace` e confirme.

O workflow possui apenas a permissão `contents: read`. Ele não publica código, cria releases nem envia alterações para qualquer remote.

### Acompanhar logs e baixar o artifact

Abra a execução em **Actions**, selecione o job **Windows x86_64 editor** e expanda cada etapa para acompanhar os logs. O resultado só é publicado depois de o arquivo existir e passar por `--version`.

Ao final, na página da execução, use a seção **Artifacts** para baixar `godot-ai-workspace-windows-x64`. O artifact contém apenas `godot.windows.editor.x86_64.exe` e tem retenção inicial de 7 dias.

Extraia o ZIP baixado para uma pasta própria e execute no PowerShell:

```powershell
& "C:\caminho\para\godot.windows.editor.x86_64.exe" --version
& "C:\caminho\para\godot.windows.editor.x86_64.exe"
```

Use a versão modificada somente com projetos e cópias autorizados. O executável não substitui a instalação oficial da Godot.

### Diagnóstico de falhas

- **Falha em Setup Python and SCons:** confirme no log a versão do Python e a instalação de SCons feita pela ação local `godot-deps`.
- **Falha ao detectar MSVC:** confirme que o runner é `windows-latest` e que o log não indica incompatibilidade com o Windows SDK.
- **Falha em dependência opcional:** mantenha os switches `d3d12=no`, `accesskit=no`, `angle=no` e `winrt=no`; eles evitam dependências externas nesta primeira versão.
- **Executável ausente:** verifique a etapa de compilação e o conteúdo de `bin/`. A etapa `Verify editor executable` falha explicitamente quando `bin/godot.windows.editor.x86_64.exe` não existe.
- **Falha no `--version`:** trate como build não validado; o artifact não deve ser considerado utilizável.

Falhas de compilação devem ser reproduzidas observando a primeira mensagem de erro do compilador, não apenas o resumo final do job. Depois de corrigir C++ ou configuração, faça um novo commit e execute novamente o workflow manual.

### Cache do SCons

O workflow reutiliza as ações oficiais locais `godot-cache-restore` e `godot-cache-save`, com a chave `ai-workspace-windows-editor`. O cache contém somente a pasta de cache do SCons e pode acelerar compilações posteriores. A restauração e o salvamento são tolerantes a falhas para que um problema de cache não oculte uma falha real de compilação. Não foi adicionada uma infraestrutura de cache paralela.

## Publicação em um repositório próprio

O repositório local atualmente não possui `origin`. Para publicar a branch, crie primeiro um repositório próprio no GitHub — preferencialmente público se isso for compatível com a política e a licença aplicáveis — e, somente após autorização explícita:

```powershell
git remote add origin https://github.com/SEU_USUARIO/godot-ai-workspace.git
git push -u origin ai-workspace
```

Não use o repositório oficial da Godot como `origin` e não execute `git push` sem autorização. O remote `upstream` deve continuar apontando para a Godot oficial.

Depois de futuras alterações em C++, confirme o estado local, faça commit apenas das alterações pretendidas e repita a execução manual do workflow. Compare o log, confirme `--version` e baixe o novo artifact antes de utilizá-lo.

## Etapa 09 - integração Codex

O workflow Windows compila também o adaptador nativo em `editor/ai_workspace/agents/codex/`. A execução local não exige compilador C++ nem SCons: abra o artifact gerado pelo GitHub Actions, confirme `--version` e use a aba **AI Workspace** para pressionar **Connect Codex**.

O estado `Not installed` significa que `codex --version` não foi encontrado no `PATH`; `Authentication required` significa que o app-server iniciou, mas recusou a sessão por autenticação. O editor não lê nem armazena tokens. Logs do app-server não são copiados para o projeto e não devem ser compartilhados se contiverem dados sensíveis.

Para repetir após alterações em C++, faça commit e push da branch `ai-workspace`, inicie manualmente `AI Workspace - Windows Build`, aguarde a conclusão, baixe `godot-ai-workspace-windows-x64` e execute `--version` e `--headless --editor --quit`. A validação de uma sessão Codex real requer uma máquina com o CLI instalado e autenticado; não declare essa parte aprovada sem evidência do processo e da resposta.

## Etapa 07 - cenas e recursos

O Command Router também aceita os comandos estruturados abaixo na área técnica do AI Workspace:

```json
{"command":"scene.create","args":{"root_type":"Node2D","root_name":"Level","path":"res://scenes/level.tscn"}}
{"command":"scene.open","args":{"path":"res://scenes/level.tscn"}}
{"command":"scene.save","args":{}}
{"command":"resource.create","args":{"type":"Gradient","path":"res://resources/my_gradient.tres"}}
{"command":"resource.assign","args":{"node_path":"Sprite","property":"texture","resource_path":"res://textures/player.tres"}}
```

As operações de criação aceitam somente `Node2D`/`Node3D` para raízes e `Gradient`/`GradientTexture2D` para recursos `.tres`. A associação inicial aceita somente `Sprite2D.texture` com `Texture2D`. Caminhos precisam começar por `res://`, ser normalizados, não conter `..`, caminhos absolutos, `user://`, separadores alternativos, protocolos adicionais ou componentes simbólicos. Arquivos existentes são recusados e não há comando de exclusão.

`resource.assign` é registrado no Undo/Redo nativo e restaura o recurso anterior. `scene.open` bloqueia a operação quando o histórico da cena atual está não salvo, evitando descarte silencioso. `scene.save` salva apenas a cena atual que já possui caminho; não abre Save As automaticamente e não promete Undo/Redo para a gravação em disco. Criação de cenas e recursos grava diretamente por `PackedScene`, `ResourceSaver` e APIs nativas equivalentes, sem modificar arquivos externos.

## Etapa 05 - contexto nativo do projeto

O commit inicial `fa64de55389` adicionou o serviço nativo de contexto, a atualização por seleção/cena e a inspeção JSON somente leitura. A primeira compilação remota ([run 43989803900](https://github.com/ps25br/godot-ai-workspace/actions/runs/43989803900)) passou e gerou o artifact. O executável confirmou `4.7.2.stable.gh.fa64de553`.

Durante a validação headless foram encontrados dois problemas de ciclo de vida: os sinais de cena eram conectados antes do registro completo do editor e a leitura da cena podia consultar um índice vazio. Os commits `73dde725150`, `5766ced11e9` e `e46c76e4364` corrigem respectivamente a origem dos sinais, o include de `callable_mp` e o adiamento da conexão para `make_visible`, além da guarda para nenhuma cena aberta.

As três tentativas autorizadas desta etapa foram usadas. A terceira compilação ([run 43990748072](https://github.com/ps25br/godot-ai-workspace/actions/runs/43990748072)) passou e publicou o artifact, mas corresponde ao commit `5766ced11e9`; a correção final `e46c76e4364` foi publicada depois e ainda precisa de uma nova compilação remota. Portanto, a aba e o JSON não devem ser considerados visualmente validados nesta etapa. Não houve compilação C++ local.

## Execução remota verificada

## Etapa 05.1 - estabilização e validação do contexto

O HEAD `4043f6262f1` foi compilado com sucesso na execução [43991944127](https://github.com/ps25br/godot-ai-workspace/actions/runs/43991944127). O artifact foi baixado em `C:\Users\win10\Documents\Godot AI Workspace-artifacts\run-43991944127\godot.windows.editor.x86_64.exe` e confirmou `4.7.2.stable.gh.4043f6262` com `--version` e código de saída 0.

Validações headless executadas contra o projeto temporário `C:\Users\win10\Documents\Godot AI Workspace-artifacts\context-test-project`:

- `--headless --editor --quit`: código 0, stderr vazio.
- `--headless --editor --path <projeto> --quit`: código 0, stderr vazio.
- A variante exploratória `--quit-after 10` apresentou falha de encerramento do processo (`-1073741819`) e uma mensagem de singleton nulo; ela não foi usada como critério de aprovação, pois o comando oficial solicitado com `--quit` passou sem a mensagem.

Validação visual realizada em `visual-ai-workspace-active.png`, `visual-context-selection.png`, `visual-context-json-data.png` e `visual-after-close-attempt.png`, na mesma pasta do artifact. Foram confirmados: aba AI Workspace, projeto e cena atuais, seleção de `Sprite (Sprite2D)`, um recurso vinculado, JSON somente leitura e atualização para “No scene open” após fechar a cena. O editor foi encerrado graficamente sem salvar alterações no projeto temporário.

Não foram observados erros de conexão de sinais, referências inválidas ou mensagens de ciclo de vida no teste aprovado. Não houve alteração de código nesta subetapa e não houve compilação C++ local.

A execução final desta validação ([run 43993475255](https://github.com/ps25br/godot-ai-workspace/actions/runs/43993475255)) compilou o HEAD `00098739cf8` e confirmou `4.7.2.stable.gh.00098739c`. O projeto temporário original, após a sessão gráfica, reteve estado em `.godot/editor` e reproduziu uma falha de encerramento ao reabrir o layout em headless. Um projeto temporário limpo, sem `.godot`, passou o mesmo comando `--headless --editor --path <projeto> --quit` com código 0 e stderr vazio.

## Etapa 05.2 - falha intermitente com estado persistido

O cenário foi repetido com o artifact do commit `390e6b2400e` e com o executável oficial `Godot_v4.7.2-stable_win64.exe`, versão `4.7.2.stable.official.ed1daf0bf`. Ambos apresentaram, de forma intermitente, o mesmo código de exceção do Windows `0xC0000005` ao executar `--headless --editor --path <projeto> --quit` durante uma transição de cache/estado do editor. Repetições posteriores com o mesmo binário e o estado já regravado encerraram com código 0.

A existência de `.godot/editor` isoladamente não é suficiente para causar a falha: duas inicializações controladas e cinco repetições do fork passaram com stderr vazio. Copiar apenas o `editor_layout.cfg`, inclusive com a tela AI Workspace selecionada e sem cenas abertas, também não reproduziu o problema. Alternar somente o caminho de duas cópias idênticas do executável oficial passou quatro vezes. A falha depende de uma condição de corrida durante a inicialização e o encerramento imediato do editor, não de um valor isolado do layout.

O Event Viewer confirmou a violação de acesso dentro do próprio executável oficial, no offset `0x194aeb7`, além das ocorrências no fork. A mensagem `EditorNode::is_cmdline_mode` indica que algum trabalho pendente chegou a consultar o singleton depois de ele ter sido zerado no destrutor de `EditorNode`. Os chamadores existentes nessa versão são a carga do cache de documentação e `EditorFileSystem::_process_update_pending()`. Sem PDB, dump ou depurador Windows disponível, não foi possível obter uma pilha simbólica que identifique qual callback causou a violação; a mensagem pode ser secundária ao crash.

Como a Godot oficial reproduz a falha e os testes não relacionaram o problema ao ciclo de vida de `AIWorkspaceEditorPlugin`, `AIWorkspaceMainScreen` ou `AIWorkspaceContextService`, nenhum código do fork foi alterado para mascará-la. Os logs controlados foram preservados fora do repositório em `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage-05-2-logs`.

O repositório público está disponível em [github.com/ps25br/godot-ai-workspace](https://github.com/ps25br/godot-ai-workspace), com `ai-workspace` como branch padrão.

- Workflow: [AI Workspace - Windows Build](https://github.com/ps25br/godot-ai-workspace/actions/workflows/ai-workspace-windows.yml)
- Execução aprovada: [run 43961599237](https://github.com/ps25br/godot-ai-workspace/actions/runs/43961599237)
- Artifact: `godot-ai-workspace-windows-x64`, confirmado com retenção de 7 dias.
- Versão verificada: `4.7.2.stable.gh.0c6cb3abc`.
- `--version`: código de saída 0.
- Inicialização local headless sem projeto externo (`--headless --editor --quit`): código de saída 0.
- Executável baixado em: `C:\Users\win10\Documents\Godot AI Workspace-artifacts\run-43961599237\godot.windows.editor.x86_64.exe`.

A primeira execução (`43955729705`) compilou o executável, mas falhou na verificação por tratar um `$LASTEXITCODE` nulo do PowerShell como erro. A verificação foi corrigida no commit `0c6cb3a`; a segunda execução passou e publicou o artifact. Nenhuma alteração no runtime foi necessária.

Para a etapa 03, a primeira execução ([run 43963341537](https://github.com/ps25br/godot-ai-workspace/actions/runs/43963341537)) falhou por uma dependência de include: o novo arquivo precisava da definição completa de `EditorMainScreen`. O include foi corrigido no commit `90bf29d`; a compilação aprovada foi [run 43963807986](https://github.com/ps25br/godot-ai-workspace/actions/runs/43963807986).

- Artifact da etapa 03: `godot-ai-workspace-windows-x64`.
- Versão verificada: `4.7.2.stable.gh.90bf29d3b`.
- `--version`: código de saída 0.
- Inicialização headless sem projeto externo: código de saída 0.
- Executável baixado em: `C:\Users\win10\Documents\Godot AI Workspace-artifacts\run-43963807986\godot.windows.editor.x86_64.exe`.
### Validação executada

O commit `db712c98765` passou no workflow [44005636288](https://github.com/ps25br/godot-ai-workspace/actions/runs/44005636288), com artifact baixado em `C:\Users\win10\Documents\Godot AI Workspace-artifacts\run-44005636288\godot.windows.editor.x86_64.exe`. A versão reportada foi `4.7.2.stable.gh.db712c987`. O executável foi exercitado com projeto temporário, incluindo teste gráfico da aba, persistência da associação no Inspector e três execuções headless consecutivas com código 0 e stderr vazio após a sessão gráfica. Uma execução headless inicial reproduziu a falha conhecida de estado persistido (`EditorNode::is_cmdline_mode`); ela não foi ocultada e as execuções subsequentes passaram.
## Agent Bridge local

### Etapa 10.1 - probe e limites de validação

O teste independente `tools/ai_workspace_codex_probe.py` usa leitura assíncrona de stdout/stderr e timeouts separados para inicialização, primeiro evento, inatividade e duração total. Ele envia `Responda apenas: OK.` e imprime somente tempos, nomes de eventos e estados.

No Codex autenticado local, uma execução normal confirmou `initialize` em 773 ms, `thread/start` em 2261 ms, primeiro evento em 2262 ms, primeiro delta em 35603 ms, `turn/completed=completed` em 35707 ms e `thread/resume` em 36965 ms. A execução com `--interrupt-after-delta` confirmou primeiro delta em 51909 ms, `turn/completed=interrupted` em 52330 ms e resposta de `turn/interrupt` recebida.

O atraso de dezenas de segundos até o primeiro delta é observado no App Server real e não deve ser tratado como falha imediata. A GUI compilada inicializou OpenGL, mas não manteve uma janela com título capturável neste ambiente; portanto não há screenshot nem aprovação visual automatizada nesta subetapa.

### Validação da etapa 10

O commit `4d2fde81c47` passou na execução [44085486259](https://github.com/ps25br/godot-ai-workspace/actions/runs/44085486259). O artifact está em `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage10-run-44085486259\godot.windows.editor.x86_64.exe`; `--version` retornou `4.7.2.stable.gh.4d2fde81c` com código 0 e o teste `--headless --editor --path <projeto-temporário> --quit` retornou código 0.

O probe real do Codex autenticado confirmou `initialize`, `thread/start` e `thread/resume`; o turno não entregou delta e conclusão dentro do timeout e a tentativa gráfica encerrou sem janela capturável. Assim, o build está aprovado, mas os testes de conversa contínua, cancelamento e screenshots permanecem limitações desta execução, não resultados simulados.

## Etapa 10 - chat nativo do Codex

O chat usa o mesmo executável editor compilado remotamente e inicia `codex app-server --listen stdio://` somente quando **Connect Codex** é pressionado. O workflow não instala o Codex nem executa inferência durante a compilação; a máquina que abrir o artifact precisa ter o CLI instalado e autenticado.

Para validar uma alteração C++: revise o diff e execute `git diff --check`; faça commit e push de `ai-workspace`; inicie o workflow **AI Workspace - Windows Build** nessa branch; acompanhe os logs; baixe `godot-ai-workspace-windows-x64` fora do repositório; execute `godot.windows.editor.x86_64.exe --version`; e então rode `--headless --editor --path <projeto-temporário> --quit`. Para teste real, abra o editor, conecte o Codex, envie uma pergunta curta e confirme deltas, `turn/completed`, **Stop**, **New Chat** e a retomada após reabrir o editor.

O histórico visual é salvo como metadado do projeto, limitado a 64 mensagens/32768 caracteres, sem tokens. O cancelamento envia `turn/interrupt`; uma proposta nunca é executada automaticamente. A listagem de threads remotas permanece desativada nesta etapa porque a resposta local não inclui o histórico visual completo para uma troca segura. Registre sempre o commit compilado, a URL da execução, o artifact, a saída de `--version`, a saída headless e screenshots; não considere o build aprovado apenas por existir.

O Bridge é desativado por padrão. No editor compilado, abra **AI Workspace** e pressione **Enable Local Bridge**. A interface mostra o endereço loopback, o segredo da sessão e os botões de aprovação. Para testes automatizados, use a ativação explícita por linha de comando:

```powershell
& ".\godot.windows.editor.x86_64.exe" --editor --path "C:\projeto-de-teste" --ai-workspace-bridge --ai-workspace-bridge-secret="segredo-de-teste"
python tools/ai_workspace_bridge_client.py --secret segredo-de-teste self-test
```

O segredo de teste deve ser temporário. A conexão não autoriza comandos mutáveis: cada `command.execute` aguarda **Approve Command**. O cliente de teste e o contrato completo estão em `tools/ai_workspace_bridge_client.py` e `docs/agent-bridge-protocol.md`.

### Validação executada na etapa 08

O commit `d70fac7b638` passou no workflow [44011040757](https://github.com/ps25br/godot-ai-workspace/actions/runs/44011040757). O artifact confirmou `4.7.2.stable.gh.d70fac7b6`; `--version` e `--headless --editor --path <projeto> --quit` terminaram com código 0 e stderr vazio. O cliente Python validou ping, contexto, autenticação, erros de protocolo/JSON/método, limite de mensagem, aprovação de `node.create`, Undo/Redo e desconexão. A interface foi capturada em `C:\Users\win10\Documents\Godot AI Workspace-artifacts\stage08-after-approval-attempt.png`.
