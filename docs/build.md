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

## Etapa 05 - contexto nativo do projeto

O commit inicial `fa64de55389` adicionou o serviço nativo de contexto, a atualização por seleção/cena e a inspeção JSON somente leitura. A primeira compilação remota ([run 43989803900](https://github.com/ps25br/godot-ai-workspace/actions/runs/43989803900)) passou e gerou o artifact. O executável confirmou `4.7.2.stable.gh.fa64de553`.

Durante a validação headless foram encontrados dois problemas de ciclo de vida: os sinais de cena eram conectados antes do registro completo do editor e a leitura da cena podia consultar um índice vazio. Os commits `73dde725150`, `5766ced11e9` e `e46c76e4364` corrigem respectivamente a origem dos sinais, o include de `callable_mp` e o adiamento da conexão para `make_visible`, além da guarda para nenhuma cena aberta.

As três tentativas autorizadas desta etapa foram usadas. A terceira compilação ([run 43990748072](https://github.com/ps25br/godot-ai-workspace/actions/runs/43990748072)) passou e publicou o artifact, mas corresponde ao commit `5766ced11e9`; a correção final `e46c76e4364` foi publicada depois e ainda precisa de uma nova compilação remota. Portanto, a aba e o JSON não devem ser considerados visualmente validados nesta etapa. Não houve compilação C++ local.

## Execução remota verificada

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
