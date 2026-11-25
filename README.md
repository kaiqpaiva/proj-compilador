# Compiladores – Projeto Fase 2: MiniVisualg

Compilador da linguagem MiniVisualg, um subconjunto do Visualg definido pelos exemplos do Anexo I do enunciado. Faz as análises léxica, sintática e semântica e gera código intermediário para a máquina MEPA.

## Integrantes

| Nome | RA |
|---|---|
| Caio Ariel Cardoso Saraiva | 10439611 |
| Isabela Hissa Pinto | 10441873 |
| Kaique Barros Paiva | 10441787 |

## 1. O que foi concluído

**Fase 1**

- [x] Etapa 1 – Expressões regulares e gramática ([docs/etapa1_gramatica.md](docs/etapa1_gramatica.md))
- [x] Etapa 2 – Analisador léxico
- [x] Etapa 3 – Analisador sintático

**Fase 2**

- [x] Análise semântica: as 9 regras do enunciado, com a minitabela de símbolos `TNo` em lista encadeada e a função `busca_tabela_simbolos()`
- [x] Geração de código MEPA para comandos, expressões, procedimentos e funções, com a função `proximo_rotulo()` ([docs/etapa2.md](docs/etapa2.md))
- [ ] Geração de código para o operador `mod`, para o acesso a vetores e para textos fora do `escreva`: não implementada. O compilador para com a mensagem `NAO SUPORTADO` (ver [docs/etapa2.md](docs/etapa2.md), seção 4)

## 2. Como compilar e executar

### 2.1 Compilar

```sh
gcc -g -Og -Wall compilador.c -o compilador
```

O programa compila sem nenhum warning com o comando do enunciado.

### 2.2 Executar

```sh
./compilador <arquivo.alg>
```

**Testes que rodam**

Terminam com `Compilacao concluida sem erros. Codigo MEPA gerado em saida_mepa.txt`.

```sh
./compilador testes/validos/01_primeiro_passo.alg
./compilador testes/validos/02_variaveis.alg
./compilador testes/validos/03_operadores.alg
./compilador testes/validos/04_controle.alg
./compilador testes/validos/06_procedimento_funcao.alg
./compilador testes/validos/07_amem_varias_secoes.alg
./compilador testes/validos/08_amem_parametros.alg
./compilador testes/validos/09_expressoes.alg
./compilador testes/validos/10_leia.alg
./compilador testes/validos/11_escreva.alg
./compilador testes/validos/12_se.alg
./compilador testes/validos/13_enquanto.alg
./compilador testes/validos/14_para.alg
./compilador testes/validos/15_proced_func.alg
```

**Testes com erro**

Terminam com uma mensagem de erro (ou `NAO SUPORTADO`) e não geram `saida_mepa.txt`. O `05_repeticao_vetor` é um programa válido, mas usa acesso a vetor, que não tem geração de código.

```sh
./compilador testes/validos/05_repeticao_vetor.alg
./compilador testes/erros/lexico_simbolo_invalido.alg
./compilador testes/erros/lexico_string_aberta.alg
./compilador testes/erros/naosuportado_texto_atribuicao.alg
./compilador testes/erros/semantico_condicao_enquanto.alg
./compilador testes/erros/semantico_condicao_se.alg
./compilador testes/erros/semantico_funcao_como_procedimento.alg
./compilador testes/erros/semantico_funcao_sem_retorne.alg
./compilador testes/erros/semantico_nao_declarada.alg
./compilador testes/erros/semantico_nao_usada.alg
./compilador testes/erros/semantico_numero_argumentos.alg
./compilador testes/erros/semantico_palavra_reservada.alg
./compilador testes/erros/semantico_para_limite.alg
./compilador testes/erros/semantico_para_variavel_real.alg
./compilador testes/erros/semantico_passo_variavel.alg
./compilador testes/erros/semantico_passo_zero.alg
./compilador testes/erros/semantico_procedimento_em_expressao.alg
./compilador testes/erros/semantico_redeclaracao.alg
./compilador testes/erros/semantico_retorne_fora_funcao.alg
./compilador testes/erros/semantico_proced_func_duplicado.alg
./compilador testes/erros/semantico_proced_func_nome_variavel.alg
./compilador testes/erros/semantico_tipo_argumento.alg
./compilador testes/erros/semantico_tipo_atribuicao.alg
./compilador testes/erros/semantico_tipo_retorno.alg
./compilador testes/erros/semantico_tipos_expressao.alg
./compilador testes/erros/semantico_variavel_nome_proced_func.alg
./compilador testes/erros/sintatico_falta_entao.alg
./compilador testes/erros/sintatico_falta_fimpara.alg
./compilador testes/erros/sintatico_procedimento_sem_parametro.alg
```

**Todos de uma vez**

```sh
for f in testes/*/*.alg; do echo "== $f"; ./compilador $f | tail -1; done
```

### 2.3 Saídas

| Arquivo | Conteúdo |
|---|---|
| tela e `saida_tokens.txt` | listagem de tokens, no formato `linha# NOME | atributo`, e o resultado da compilação |
| `saida_mepa.txt` | código MEPA gerado, com as variáveis referenciadas pelo nome |

Ao final aparece `Compilacao concluida sem erros. Codigo MEPA gerado em saida_mepa.txt` ou uma mensagem de `ERRO LEXICO`, `ERRO SINTATICO`, `ERRO SEMANTICO` ou `NAO SUPORTADO` com a linha do erro. Em qualquer erro, o `saida_mepa.txt` é apagado, para nunca sobrar um código incompleto que pareça válido.

Para executar o código gerado, use o interpretador de teste `ferramentas/mepa.py` (ver [docs/etapa2.md](docs/etapa2.md), seção 7):

```sh
python3 ferramentas/mepa.py saida_mepa.txt
python3 ferramentas/mepa.py saida_mepa.txt --entrada 4,7   # valores para os leia
```

## 3. Decisões de design

### Geral

- Tudo fica em um único `compilador.c`, porque o comando de compilação do enunciado compila só esse arquivo.
- **Código de saída:** o programa termina com `0` quando a compilação vai até o fim e com `1` em qualquer erro (de abertura de arquivo, léxico, sintático, semântico ou de recurso não suportado).

### Analisador léxico

- Criamos um token para cada palavra reservada (`TOKEN_SE`, `TOKEN_ENTAO`, ...) em vez de um `TOKEN_KEYWORD` único. É a mesma ideia que o enunciado já aplica em `TOKEN_OP_REL` com o `OpRelAtributo`: quando o parser precisa distinguir, vira token próprio; quando não precisa, vira atributo. Por isso o `TOKEN_KEYWORD` não aparece no nosso enum.
- Incluímos `OP_NE` na lista de operadores relacionais, porque `<>` aparece nos exemplos do anexo. Vale notar também que a igualdade no Visualg é `=`, e não `==`.
- As palavras reservadas são reconhecidas sem diferenciar maiúsculas de minúsculas, já que o anexo escreve `MOD` e `E` em maiúsculo e o resto em minúsculo.
- O enunciado garante que os lexemas vêm separados por espaço. Como alguns exemplos do anexo aparecem sem espaço (`escreval("...")`), deixamos o léxico preparado para os dois casos.
- `obter_token` é uma casca fina sobre `reconhecer_token` e já registra o token na listagem. Assim a saída é gerada em uma passagem só, com o parser chamando o léxico como na Figura 1. Isso funciona porque a gramática é LL(1) e nenhum token é lido duas vezes.
- `info_token` monta a linha num buffer passado pelo chamador, em vez de um `static` interno. Dessa forma duas chamadas no mesmo `printf` não sobrescrevem uma à outra.
- As cadeias são guardadas na tabela de símbolos com as aspas, para que a cadeia `"idade"` e a variável `idade` não ocupem o mesmo índice.
- Um lexema com mais de 255 caracteres gera `ERRO LEXICO` em vez de ser truncado. Se truncássemos, dois identificadores longos e diferentes virariam o mesmo símbolo.
- Em `1..4`, devolvemos `NUM_INT` e deixamos o `..` para a chamada seguinte. Um ponto seguido de algo que não é dígito nem ponto (como `1.`) é tratado como real malformado.
- A tabela com os nomes dos tokens é um array indexado pelo enum, com o sentinela `TOKEN_TOTAL` e um `_Static_assert` que quebra a compilação se as duas listas saírem de sincronia.
- Todas as funções de erro passam por `sair_com_erro`, que fecha os arquivos antes do `exit(1)`. Sem o `fclose`, os tokens já escritos ficariam no buffer do stdio e o arquivo sairia vazio justamente nos testes de erro.

### Analisador sintático

- **Tipo de parser:** o analisador sintático é **descendente recursivo preditivo (LL(1))**. Há uma função por não-terminal da gramática da Etapa 1, e a produção é escolhida só pelo token atual (`lookahead`), sem retrocesso.
- **Uma passagem só:** o parser dirige o léxico. Cada token é pedido com `next_token()` e impresso na hora, então a listagem, a análise e a geração de código saem numa passagem só.
- **Mensagem de erro:** a mensagem de erro sintático mostra o token esperado e o encontrado, e também é gravada em `saida_tokens.txt`.
- **Fim do arquivo:** no fim, o `main` só confere que o token atual é `EOF`, sem consumi-lo, para não pedir um token depois do fim do arquivo.

### Analisador semântico

- **Duas tabelas.** A minitabela `TNo` do enunciado guarda só as **variáveis** (e os parâmetros), com nome, endereço e tipo. Os procedimentos e funções ficam numa tabela própria (`struct ProcedFunc`), com o tipo de cada parâmetro e o tipo de retorno. A `tabela_simbolos` do léxico continua existindo: ela guarda o lexema de todo identificador que aparece no fonte, declarado ou não.
- **Endereço** é a ordem de declaração: a primeira variável tem endereço 0, a segunda 1, e assim por diante. É um contador próprio, diferente do operando do `AMEM`, que também conta as posições de vetores e o retorno das funções.
- **Acesso em dois passos.** `busca_tabela_simbolos()` recebe o nome e devolve o endereço, como pede o enunciado, ou dá erro de variável não declarada. Para consultar o tipo, `acessa_var_na_tabela_simbolos()` recebe o endereço e devolve o nó. A função de busca recebe também a linha, para que a mensagem de erro informe onde o problema está.
- **Campo `tipo` com nomes curtos.** O campo tem 7 posições, então `"inteiro"` (7 letras mais o `'\0'`) e `"caractere"` não cabem. Guardamos `"int"`, `"real"`, `"char"` e `"logico"`.
- **Nomes de variável com no máximo 15 caracteres**, o tamanho do campo `ID[16]`. Um nome maior dá erro em vez de ser truncado, porque truncar poderia fazer dois nomes diferentes virarem iguais.
- **Nomes globais.** Variáveis, parâmetros, procedimentos e funções compartilham um único espaço de nomes, porque o enunciado exige identificadores únicos em todo o código. Por isso dois parâmetros com o mesmo nome, mesmo em procedimentos ou funções diferentes, dão erro de nome repetido. O nome de uma função ocupa uma posição de memória (onde fica o valor de retorno), mas não entra na minitabela: ele é tratado pela tabela de procedimentos e funções.
- **Regra de mistura de tipos** (regra 5):

  | Operação | Aceita | Resultado |
  |---|---|---|
  | `+ - *` | números | real se algum lado for real; senão inteiro |
  | `/` | números | sempre real |
  | `\` e `mod` | só inteiros | inteiro |
  | `< > <= >=` | números (inteiro e real misturam) | lógico |
  | `= <>` | números, ou dois valores do mesmo tipo | lógico |
  | `e ou` | só lógicos | lógico |
  | menos unário | números | o mesmo tipo |

- **Atribuição, argumento e retorno** seguem a mesma regra: o valor precisa ser do mesmo tipo do destino, ou ser inteiro indo para real. Real indo para inteiro não pode, porque perderia a parte decimal.
- **Condições e `para`.** A condição do `se` e do `enquanto` precisa ser lógica. No `para`, a variável de controle, o valor inicial, o limite e o passo precisam ser inteiros.
- **Regra 4 (variável atribuída e não usada).** As marcas de "atribuída" e "usada" ficam em vetores paralelos indexados pelo endereço, e não dentro da `TNo`, porque o enunciado define a struct com exatamente quatro campos. A conferência é feita no fim do programa. Parâmetros não contam como atribuídos, porque o valor vem de quem chama.
- **Regra 2 (palavra reservada como nome).** O léxico transforma `se` em `TOKEN_SE` antes de o parser ver o token. Para que `se: inteiro` dê erro semântico, e não sintático, o parser reconhece a palavra reservada onde espera o nome de uma variável ou de um parâmetro. Na primeira posição de uma linha da seção `var`, o token seguinte decide: se for `:` ou `,`, era uma declaração (erro semântico); senão, o programa esqueceu o `inicio` (erro sintático, como antes). Uma palavra reservada no corpo do programa (`se <- 1`) ou como nome de procedimento ou função (`procedimento se`) continua dando erro sintático, porque a regra trata de nomes de variável.
- **Função sem `retorne`** é erro semântico. A conferência é só de presença: um `retorne` dentro de um `se` já conta, porque o compilador não analisa todos os caminhos de execução.
- **`leia`** aceita variável de qualquer tipo. Na MEPA, o `LEIT` lê sempre um número.

## 4. Bugs conhecidos

- **Identificadores com acento:** não são suportados. O alfabeto da Etapa 1 define letras como `a-zA-Z`, então `media` funciona e `média` é cortada no acento. Isso gera um erro léxico cuja sequência exibida sai ilegível (byte UTF-8 solto). Acentos dentro de cadeias e comentários funcionam normalmente.
- **Inteiros muito grandes:** um literal inteiro maior que o limite de `int` é convertido por `strtol` e depois para `int`, sem aviso, e o valor sai errado. Não há verificação de overflow.
- **Recursão:** compila, mas não executa corretamente, porque os parâmetros são posições globais. As demais limitações da geração de código estão em [docs/etapa2.md](docs/etapa2.md), seção 4.
