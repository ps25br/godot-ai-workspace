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
git describe --tags --exact-match HEAD
git rev-parse HEAD
```

