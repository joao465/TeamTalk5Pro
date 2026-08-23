# TeamTalk 5 Pro — fork com EQ, microfone secundário e atualização automática

Este repositório é um fork do [TeamTalk 5](https://github.com/BearWare/TeamTalk5), mantido originalmente pela BearWare.dk.

O objetivo deste fork é manter a base do TeamTalk compatível com o projeto original e adicionar recursos específicos ao cliente Qt para Windows, com foco em áudio, acessibilidade, distribuição portátil e atualização simples.

> O código original do TeamTalk pertence aos respectivos autores e continua sujeito às licenças existentes no projeto. Este README descreve as modificações feitas neste fork.

## Principais recursos adicionados

- **TeamTalk 5 Pro para Windows x64** com identidade e instalador próprios.
- **Equalizador do microfone principal** integrado ao cliente Qt.
- **Microfone/entrada secundária**, permitindo selecionar um segundo dispositivo de entrada.
- **Escuta do microfone secundário** diretamente nas preferências.
- O **EQ é aplicado somente ao microfone principal**.
- O áudio do microfone secundário é misturado **depois do EQ**, portanto não é alterado pelo equalizador do microfone principal.
- Configurações separadas do TeamTalk oficial, usando `TeamTalk5Pro.ini`.
- Instalador independente em Inno Setup.
- Pacote portátil para Windows x64.
- **Atualizador automático baseado em GitHub Releases**.
- Workflow rápido de compilação do cliente Windows.

## Atualizador automático

O cliente verifica a última Release pública deste repositório usando a API do GitHub:

`https://api.github.com/repos/joao465/TeamTalk5/releases/latest`

Quando uma versão mais nova é encontrada, o usuário recebe a mensagem:

```text
Uma nova versão de TeamTalk está disponível!

Versão atual: 5.26.2
Nova versão: 5.26.3

Deseja atualizar agora?
```

Se o usuário escolher **Sim**, o programa:

1. localiza o instalador Windows x64 anexado à Release;
2. baixa o arquivo para a pasta temporária do Windows;
3. mostra o progresso do download;
4. verifica o SHA-256 quando o GitHub disponibiliza o digest do asset;
5. inicia o instalador;
6. fecha o TeamTalk para permitir a atualização dos arquivos.

Erros de rede durante a verificação automática não interrompem a inicialização do TeamTalk. O item **Check for Update** do menu também usa o novo atualizador.

### Regra de versão

O cliente compara `APPVERSION_SHORT`, definido em:

`Client/qtTeamTalk/appinfo.h`

com a versão da tag da última Release. São aceitas tags como:

- `pro-v5.26.3`
- `v5.26.3`
- `5.26.3`

Para atualização automática, a nova Release precisa ter uma versão numericamente maior do que a instalada.

## Como publicar uma nova versão sem enviar arquivos manualmente

Existe o workflow:

`.github/workflows/publish-pro-release.yml`

A forma recomendada é criar uma tag no commit que será publicado:

```bash
git checkout teamtalk-pro-mic-eq
git pull
git tag pro-v5.26.3
git push origin pro-v5.26.3
```

O GitHub Actions então:

1. obtém a versão `5.26.3` a partir da tag;
2. grava essa versão temporariamente no build do cliente;
3. compila o TeamTalk Pro para Windows x64;
4. gera o pacote portátil;
5. gera `TeamTalk_5_Pro_5.26.3_Setup_x64.exe`;
6. cria a Release `pro-v5.26.3`;
7. anexa o instalador e o ZIP portátil;
8. marca a Release como a versão mais recente.

A partir desse momento, instalações antigas detectam a Release automaticamente.

Também existe `workflow_dispatch` no workflow de publicação. Depois que este workflow estiver disponível na branch padrão do repositório, ele poderá ser iniciado pela aba **Actions**, informando somente o número da versão.

> A primeira versão que contém o novo atualizador precisa ser distribuída normalmente uma vez. Depois disso, versões futuras podem ser entregues pelo próprio atualizador.

## Como o microfone secundário foi implementado

A implementação atravessa a interface Qt, a API C do TeamTalk e o `ClientNode`.

### Interface e configurações

Arquivos principais:

- `Client/qtTeamTalk/settings.h`
- `Client/qtTeamTalk/utilsound.h`
- `Client/qtTeamTalk/utilsound.cpp`
- `Client/qtTeamTalk/preferences.ui`
- `Client/qtTeamTalk/preferencesdlg.h`
- `Client/qtTeamTalk/preferencesdlg.cpp`

A interface possui:

- **Microfone principal**;
- **Microfone secundário**;
- **Escuta do mic secundário**.

O dispositivo secundário é salvo separadamente nas configurações do cliente.

### API do TeamTalk

Foram adicionadas funções para inicializar e fechar a segunda entrada de áudio em:

- `Library/TeamTalk_DLL/TeamTalk.h`
- `Library/TeamTalkLib/bin/dll/TeamTalk.cpp`

As funções expostas incluem:

```cpp
TT_InitSecondarySoundInputDevice(...)
TT_CloseSecondarySoundInputDevice(...)
```

### Captura e mistura

A implementação principal fica em:

- `Library/TeamTalkLib/teamtalk/client/ClientNode.h`
- `Library/TeamTalkLib/teamtalk/client/ClientNode.cpp`

O segundo dispositivo possui captura e buffer próprios. Quando necessário, o áudio é convertido para o formato usado pelo codec antes da mistura.

A ordem é intencional:

```text
Microfone principal
        |
        v
Equalizador Pro
        |
        v
Mistura <----- Microfone secundário
        |
        v
Codificação/transmissão de voz
```

Assim, o microfone secundário **não passa pelo EQ do microfone principal**.

## Como compilar no GitHub Actions

Para Windows, o caminho mais simples é usar:

`.github/workflows/windows-fast-client.yml`

O workflow **Windows Fast Client** prepara automaticamente:

- Qt;
- MSYS2;
- NASM;
- Inno Setup;
- toolchain do TeamTalk;
- cache incremental;
- cliente Qt;
- instalador;
- pacote portátil.

Quando concluído com sucesso, os artifacts incluem o instalador e a versão portátil.

## Como compilar localmente no Windows

A compilação completa do TeamTalk possui várias dependências. O workflow `windows-fast-client.yml` deve ser usado como referência atualizada para as versões e opções utilizadas neste fork.

### Requisitos principais

- Windows x64;
- Git;
- CMake;
- Visual Studio com ferramentas C++ x64;
- Qt 6 com `qtmultimedia` e `qtspeech`;
- MSYS2;
- NASM;
- Inno Setup 6, caso queira gerar o instalador.

O build automatizado atualmente usa Qt **6.11.1** e NASM **3.01**.

### Clonar

```bash
git clone https://github.com/joao465/TeamTalk5.git
cd TeamTalk5
git checkout teamtalk-pro-mic-eq
git submodule update --init --recursive
```

### Configurar o cliente

Em um terminal com o ambiente C++ do Visual Studio disponível:

```powershell
cmake -S . -B ..\output-fast -A x64 `
  -DBUILD_TEAMTALK_LIBRARIES=OFF `
  -DBUILD_TEAMTALK_SERVERS=OFF `
  -DBUILD_TEAMTALK_DOCUMENTATION=OFF `
  -DFEATURE_WEBRTC=OFF `
  -DTOOLCHAIN_BUILD_EXTERNALPROJECTS=ON `
  -DTOOLCHAIN_INSTALL_PREFIX=..\install-toolchain
```

### Compilar

```powershell
cmake --build ..\output-fast --parallel --config Release --target TeamTalk5
cmake --build ..\output-fast --parallel --config Release --target QtTeamTalk5-windeploy-release
```

O executável Qt é produzido na árvore `Client/qtTeamTalk`, juntamente com os arquivos preparados pelo `windeployqt`.

Na primeira compilação, a toolchain pode demorar bastante. Os workflows deste fork utilizam cache para reduzir o tempo das compilações seguintes.

## Instalador

O script do instalador está em:

`Setup/Installer/Windows/TeamTalkPro.iss`

Ele aceita a versão por parâmetro:

```text
-dProVersion=5.26.3
```

Exemplo com Inno Setup:

```powershell
ISCC.exe "-dClientDir=C:\caminho\para\cliente" "-dProVersion=5.26.3" Setup\Installer\Windows\TeamTalkPro.iss
```

O resultado é um arquivo semelhante a:

`TeamTalk_5_Pro_5.26.3_Setup_x64.exe`

## Estrutura mais importante deste fork

```text
Client/qtTeamTalk/                 Cliente Qt e interface
Library/TeamTalk_DLL/              API pública TeamTalk
Library/TeamTalkLib/               Captura, processamento e mistura de áudio
Setup/Installer/Windows/           Instalador Pro
.github/workflows/windows-fast-client.yml
.github/workflows/publish-pro-release.yml
```

## Projeto original

Este fork é baseado no projeto TeamTalk 5 da BearWare.dk:

- Projeto original: https://github.com/BearWare/TeamTalk5
- Site do TeamTalk: https://www.bearware.dk

Consulte também os arquivos de licença existentes no repositório antes de redistribuir ou modificar componentes do projeto.

## Estado atual

A versão Windows x64 deste fork já foi compilada com sucesso contendo:

- equalizador do microfone principal;
- microfone secundário;
- monitoramento/escuta da entrada secundária;
- instalador Pro independente;
- pacote portátil;
- sistema de atualização por GitHub Releases.
