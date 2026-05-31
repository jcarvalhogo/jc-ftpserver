# jc-ftpserver

Servidor FTP simples escrito em C++17, com configuracao por arquivo, autenticacao por usuario/senha e suporte a transferencias em modo passivo.

Descricao breve para o GitHub:

> Servidor FTP basico e multiplataforma em C++17, criado com CMake, com login configuravel, diretorios raiz isolados e suporte aos principais comandos de listagem, upload e download em modo passivo.

## Recursos

- Configuracao externa por arquivo `.conf`.
- Login com usuario e senha.
- Diretorio raiz isolado, impedindo acesso fora da pasta configurada.
- Raizes dedicadas por alias de login, como `admin@imh`.
- Modo passivo `PASV` e `EPSV` para conexoes de dados.
- Listagem, upload, download, criacao e remocao de arquivos/diretorios.
- Uma thread por cliente conectado.
- Suporte a Linux/macOS via sockets POSIX e Windows via WinSock.

Comandos FTP implementados:

```text
USER, PASS, PWD, XPWD, CWD, CDUP, PASV, EPSV, LIST, NLST,
RETR, STOR, DELE, MKD, XMKD, RMD, XRMD, TYPE, SYST, FEAT,
OPTS, NOOP e QUIT
```

Este projeto e intencionalmente simples. Ele nao implementa TLS/FTPS, multiplas senhas, limite por IP ou modo ativo `PORT`.

## Estrutura

```text
include/jcftp/          Headers do servidor
src/                    Implementacao
ftpserver.conf          Configuracao padrao para Linux/macOS
ftpserver.windows.conf  Configuracao exemplo para Windows
CMakeLists.txt          Configuracao de build com CMake
```

## Requisitos

- CMake 3.16 ou superior.
- Compilador com suporte a C++17.
- Linux/macOS, MinGW ou Visual Studio no Windows.

## Build

No Linux/macOS:

```bash
cmake -S . -B build
cmake --build build
```

No Windows com Visual Studio:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

No Windows com MinGW ou outro gerador single-config:

```powershell
cmake -S . -B build
cmake --build build
```

## Configuracao

Por padrao, o servidor procura o arquivo `ftpserver.conf` no Linux/macOS e `ftpserver.windows.conf` no Windows.

Exemplo:

```ini
bind_address = 0.0.0.0
port = 2121

username = admin
password = admin123

root_dir = ./ftp-root
root_dir.imh = ./ftp-root/hmi
root_dir.production = ./ftp-root/production

passive_port_min = 40000
passive_port_max = 40100

backlog = 16
```

Campos principais:

- `bind_address`: endereco onde o servidor escuta. Use `0.0.0.0` para aceitar conexoes em todas as interfaces.
- `port`: porta do canal de controle FTP. A porta `2121` permite executar sem permissao de administrador/root.
- `username` e `password`: credenciais de acesso.
- `root_dir`: pasta local padrao que ficara visivel no FTP. O servidor cria a pasta se ela nao existir.
- `root_dir.<alias>`: pasta local dedicada para um alias de login. Por exemplo, `root_dir.imh` fica visivel quando o cliente entra com usuario `admin@imh` e a mesma senha de `admin`.
- `passive_port_min` e `passive_port_max`: intervalo usado para transferencias em modo passivo.
- `backlog`: tamanho da fila de conexoes pendentes.

Tambem e possivel informar outro arquivo de configuracao ao iniciar:

```bash
./build/jc_ftpserver ./meu-servidor.conf
```

No Windows, use barras `/` em caminhos absolutos:

```ini
root_dir = C:/Users/SeuUsuario/Documents/ftp-files
```

## Como executar

Linux/macOS:

```bash
./build/jc_ftpserver
```

Windows com Visual Studio:

```powershell
.\build\Release\jc_ftpserver.exe
```

Windows com MinGW ou geradores single-config:

```powershell
.\build\jc_ftpserver.exe
```

Opcoes disponiveis:

```bash
./build/jc_ftpserver --help
./build/jc_ftpserver --version
./build/jc_ftpserver ./arquivo.conf
```

## Como usar

Depois que o servidor estiver rodando, conecte usando um cliente FTP na porta configurada.

Exemplo com `curl`:

```bash
curl --user admin:admin123 ftp://127.0.0.1:2121/
```

Para acessar a raiz dedicada de HMI:

```bash
curl --user 'admin@imh:admin123' ftp://127.0.0.1:2121/
```

Enviar arquivo:

```bash
curl --user admin:admin123 -T arquivo.txt ftp://127.0.0.1:2121/arquivo.txt
```

Baixar arquivo:

```bash
curl --user admin:admin123 ftp://127.0.0.1:2121/arquivo.txt -o baixado.txt
```

Tambem e possivel usar clientes como FileZilla, WinSCP ou o cliente FTP do sistema, informando:

```text
Host: 127.0.0.1
Porta: 2121
Usuario: admin
Senha: admin123
Modo: passivo
```

## Firewall e rede

Para acessar o servidor por outra maquina, libere no firewall:

- A porta de controle configurada em `port`.
- Todo o intervalo configurado entre `passive_port_min` e `passive_port_max`.

Se o servidor estiver atras de NAT/roteador, tambem sera necessario redirecionar essas portas para a maquina que esta executando o servidor.

## Licenca

Adicione aqui a licenca desejada para o projeto.
