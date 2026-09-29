/*
 * ============================================================
 *  COMPILADORES - PROJETO FASE 1
 *  Analisador Lexico e Sintatico da linguagem MiniVisualg
 * ============================================================
 *  Integrantes:
 *    Caio Ariel Cardoso Saraiva  - RA: 10439611
 *    Isabela Hissa Pinto         - RA: 10441873
 *    Kaique Barros Paiva         - RA: 10441787
 *
 *  Compilar:
 *    gcc -Wall -Wno-unused-result -g -Og compilador.c -o compilador
 *
 *  Executar:
 *    ./compilador testes/validos/01_primeiro_passo.alg
 *
 *  Saida: tokens e resultado da analise sintatica na tela e no
 *  arquivo saida_tokens.txt
 * ------------------------------------------------------------
 *  ORGANIZACAO DO ARQUIVO
 *
 *    1. Definicao dos tokens .... enum TokenNome, OpRelType,
 *                                 struct Token e tabelas de nomes
 *    2. Estado global ........... buffer do fonte, posicao, linha,
 *                                 lookahead e tabela de simbolos
 *    3. Funcoes auxiliares ...... leitura do arquivo, tabela de
 *                                 simbolos e formatacao da saida
 *    4. Tratamento de erros ..... erroLexico e erroSintatico
 *    5. Analisador lexico ....... Etapa 2 (obterToken)
 *    6. Analisador sintatico .... Etapa 3 (nextToken e a gramatica)
 *    7. main .................... liga tudo
 *
 *  FLUXO DE EXECUCAO (Figura 1 do enunciado)
 *
 *    O parser e quem dirige a analise. Ele chama nextToken(), que
 *    chama obterToken() no lexico, que devolve o proximo token e ja
 *    registra a linha correspondente na tela e no arquivo de saida.
 *    Assim a listagem de tokens da Etapa 2 e a analise sintatica da
 *    Etapa 3 acontecem numa unica leitura do fonte.
 *
 *      main -> nextToken -> obterToken -> reconhecerToken
 *                               |
 *                               +-> imprimirToken -> infoToken
 *
 *  O programa termina com codigo 0 quando a analise vai ate o fim
 *  sem erros, e com codigo 1 em qualquer erro (de abertura de
 *  arquivo, lexico ou sintatico).
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ARQUIVO_SAIDA   "saida_tokens.txt"
#define MAX_LEXEMA      256     /* tamanho maximo de um lexema, com o '\0' */
#define MAX_SIMBOLOS    512     /* entradas da tabela de simbolos          */
#define MAX_LINHA_SAIDA 64      /* tamanho da linha formatada de um token  */

/* ============================================================
 *  1. DEFINICAO DOS TOKENS (Figura 2 do enunciado + extras)
 * ============================================================ */

/*
 * Nomes dos tokens usados pelo parser.
 *
 * Os cinco primeiros valores sao os do enunciado e mantem o mesmo
 * significado. Os demais foram acrescentados porque a linguagem tem
 * simbolos, literais e palavras reservadas que o parser precisa
 * distinguir um a um para escolher a producao certa da gramatica.
 *
 * O criterio usado foi: lexemas que aparecem no mesmo ponto da
 * gramatica viram um unico token com atributo (e o caso de TOKEN_OP_REL,
 * que guarda qual operador relacional era no campo op_code); lexemas
 * que levam a producoes diferentes viram tokens diferentes. Por isso
 * nao existe um TOKEN_KEYWORD generico aqui: reconhecer "se" com um
 * unico teste (lookahead.type == TOKEN_SE) e mais direto do que testar
 * o tipo e depois o sub-codigo da palavra reservada.
 *
 * TOKEN_TOTAL nao e um token: e um sentinela que vale a quantidade de
 * valores do enum e serve para validar o tamanho da tabela nomesTokens.
 *
 * Ver readme.txt, secao de decisoes de design.
 */
typedef enum {
      TOKEN_EOF = 0,
      TOKEN_ID,           /* identificadores (variaveis, funcoes)  */
      TOKEN_NUM_INT,      /* numeros inteiros (ex: 42)             */
      TOKEN_NUM_FLOAT,    /* numeros reais (ex: 3.14)              */
      TOKEN_OP_REL,       /* operadores relacionais                */

      /* literais */
      TOKEN_STRING,       /* "texto entre aspas"                   */

      /* operadores e pontuacao */
      TOKEN_ATRIB,        /* <-  */
      TOKEN_SOMA,         /* +   */
      TOKEN_SUB,          /* -   */
      TOKEN_MULT,         /* *   */
      TOKEN_DIV,          /* /   */
      TOKEN_DIV_INT,      /* \   */
      TOKEN_ABRE_PAR,     /* (   */
      TOKEN_FECHA_PAR,    /* )   */
      TOKEN_ABRE_COL,     /* [   */
      TOKEN_FECHA_COL,    /* ]   */
      TOKEN_VIRGULA,      /* ,   */
      TOKEN_DOIS_PONTOS,  /* :   */
      TOKEN_PONTO_PONTO,  /* ..  */

      /* palavras reservadas (um token para cada) */
      TOKEN_ALGORITMO, TOKEN_VAR, TOKEN_INICIO, TOKEN_FIMALGORITMO,
      TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE, TOKEN_LOGICO,
      TOKEN_VETOR, TOKEN_DE,
      TOKEN_SE, TOKEN_ENTAO, TOKEN_SENAO, TOKEN_FIMSE,
      TOKEN_PARA, TOKEN_ATE, TOKEN_PASSO, TOKEN_FACA, TOKEN_FIMPARA,
      TOKEN_ENQUANTO, TOKEN_FIMENQUANTO,
      TOKEN_PROCEDIMENTO, TOKEN_FIMPROCEDIMENTO,
      TOKEN_FUNCAO, TOKEN_FIMFUNCAO, TOKEN_RETORNE,
      TOKEN_LEIA, TOKEN_ESCREVA, TOKEN_ESCREVAL,
      TOKEN_VERDADEIRO, TOKEN_FALSO,
      TOKEN_E, TOKEN_OU, TOKEN_MOD,

      TOKEN_TOTAL         /* sentinela: quantidade de tokens       */
} TokenNome;

/*
 * Nome textual de cada token, usado na listagem de saida e nas
 * mensagens de erro.
 *
 * A tabela e indexada pelo proprio enum (inicializadores designados),
 * entao a ordem das linhas aqui nao importa e nenhuma posicao fica
 * "desalinhada" se um token novo for inserido no meio do enum.
 */
static const char *nomesTokens[] = {
      [TOKEN_EOF]             = "EOF",
      [TOKEN_ID]              = "ID",
      [TOKEN_NUM_INT]         = "NUM_INT",
      [TOKEN_NUM_FLOAT]       = "NUM_FLOAT",
      [TOKEN_OP_REL]          = "OP_REL",
      [TOKEN_STRING]          = "STRING",

      [TOKEN_ATRIB]           = "ATRIB",
      [TOKEN_SOMA]            = "SOMA",
      [TOKEN_SUB]             = "SUB",
      [TOKEN_MULT]            = "MULT",
      [TOKEN_DIV]             = "DIV",
      [TOKEN_DIV_INT]         = "DIV_INT",
      [TOKEN_ABRE_PAR]        = "ABRE_PAR",
      [TOKEN_FECHA_PAR]       = "FECHA_PAR",
      [TOKEN_ABRE_COL]        = "ABRE_COL",
      [TOKEN_FECHA_COL]       = "FECHA_COL",
      [TOKEN_VIRGULA]         = "VIRGULA",
      [TOKEN_DOIS_PONTOS]     = "DOIS_PONTOS",
      [TOKEN_PONTO_PONTO]     = "PONTO_PONTO",

      [TOKEN_ALGORITMO]       = "ALGORITMO",
      [TOKEN_VAR]             = "VAR",
      [TOKEN_INICIO]          = "INICIO",
      [TOKEN_FIMALGORITMO]    = "FIMALGORITMO",
      [TOKEN_INTEIRO]         = "INTEIRO",
      [TOKEN_REAL]            = "REAL",
      [TOKEN_CARACTERE]       = "CARACTERE",
      [TOKEN_LOGICO]          = "LOGICO",
      [TOKEN_VETOR]           = "VETOR",
      [TOKEN_DE]              = "DE",
      [TOKEN_SE]              = "SE",
      [TOKEN_ENTAO]           = "ENTAO",
      [TOKEN_SENAO]           = "SENAO",
      [TOKEN_FIMSE]           = "FIMSE",
      [TOKEN_PARA]            = "PARA",
      [TOKEN_ATE]             = "ATE",
      [TOKEN_PASSO]           = "PASSO",
      [TOKEN_FACA]            = "FACA",
      [TOKEN_FIMPARA]         = "FIMPARA",
      [TOKEN_ENQUANTO]        = "ENQUANTO",
      [TOKEN_FIMENQUANTO]     = "FIMENQUANTO",
      [TOKEN_PROCEDIMENTO]    = "PROCEDIMENTO",
      [TOKEN_FIMPROCEDIMENTO] = "FIMPROCEDIMENTO",
      [TOKEN_FUNCAO]          = "FUNCAO",
      [TOKEN_FIMFUNCAO]       = "FIMFUNCAO",
      [TOKEN_RETORNE]         = "RETORNE",
      [TOKEN_LEIA]            = "LEIA",
      [TOKEN_ESCREVA]         = "ESCREVA",
      [TOKEN_ESCREVAL]        = "ESCREVAL",
      [TOKEN_VERDADEIRO]      = "VERDADEIRO",
      [TOKEN_FALSO]           = "FALSO",
      [TOKEN_E]               = "E",
      [TOKEN_OU]              = "OU",
      [TOKEN_MOD]             = "MOD"
};

/* Se este assert quebrar, faltou (ou sobrou) um nome na tabela acima.
 * O erro aparece na compilacao, e nao como um nome errado na saida. */
_Static_assert(sizeof(nomesTokens) / sizeof(nomesTokens[0]) == TOKEN_TOTAL,
               "nomesTokens dessincronizado com o enum TokenNome");

/*
 * Sub-codigos do atributo dos operadores relacionais.
 *
 * Todos os operadores relacionais aparecem no mesmo ponto da gramatica
 * (a producao expr_rel), entao o parser nao precisa distinguir um do
 * outro: o tipo do token e sempre TOKEN_OP_REL e o operador exato fica
 * guardado no campo attribute.op_code, como sugere a Figura 2.
 *
 * OP_NE foi acrescentado porque o operador de diferenca "<>" aparece
 * nos exemplos do anexo. Vale lembrar que no Visualg a igualdade e um
 * "=" so, e nao "==".
 */
typedef enum {
      OP_LT,      /* <  */
      OP_LE,      /* <= */
      OP_EQ,      /* =  (no Visualg igualdade e um "=" so) */
      OP_GT,      /* >  */
      OP_GE,      /* >= */
      OP_NE,      /* <> */
      OP_TOTAL    /* sentinela: quantidade de operadores    */
} OpRelType;

/* Nome textual de cada sub-codigo de operador relacional.
 * Indexada pelo proprio enum, mesma ideia de nomesTokens. */
static const char *nomesOpRel[] = {
      [OP_LT] = "LT",
      [OP_LE] = "LE",
      [OP_EQ] = "EQ",
      [OP_GT] = "GT",
      [OP_GE] = "GE",
      [OP_NE] = "NE"
};

_Static_assert(sizeof(nomesOpRel) / sizeof(nomesOpRel[0]) == OP_TOTAL,
               "nomesOpRel dessincronizado com o enum OpRelType");

/*
 * Estrutura do token com a union de atributos (Figura 2).
 *
 * O campo 'type' diz qual dos campos da union esta valendo:
 *
 *    TOKEN_ID, TOKEN_STRING ........... table_index
 *    TOKEN_NUM_INT .................... int_value
 *    TOKEN_NUM_FLOAT .................. float_value
 *    TOKEN_OP_REL ..................... op_code
 *    demais tokens .................... nenhum (a union e ignorada)
 *
 * O campo 'line' guarda a linha do fonte em que o lexema comeca e e
 * usado tanto na listagem quanto nas mensagens de erro.
 */
typedef struct {
      TokenNome type;         /* nome do token                     */
      int line;               /* linha do fonte, para erros        */
      union {
            int table_index;    /* indice na tabela de simbolos    */
            int int_value;      /* valor literal convertido        */
            double float_value; /* valor literal convertido        */
            OpRelType op_code;  /* qual operador relacional era    */
      } attribute;
} Token;

/* Par lexema -> token usado na tabela de palavras reservadas. */
typedef struct {
      const char *lexema;
      TokenNome token;
} PalavraReservada;

/*
 * Tabela de palavras reservadas do MiniVisualg.
 *
 * O lexico primeiro reconhece qualquer sequencia de letras, digitos e
 * '_' como identificador e so depois consulta esta tabela: se o lexema
 * estiver aqui, o token vira a palavra reservada correspondente. A
 * comparacao ignora maiusculas e minusculas, porque o anexo escreve
 * "MOD" e "E" em maiusculo e o resto em minusculo.
 */
const PalavraReservada palavrasReservadas[] = {
      {"algoritmo", TOKEN_ALGORITMO}, {"var", TOKEN_VAR},
      {"inicio", TOKEN_INICIO}, {"fimalgoritmo", TOKEN_FIMALGORITMO},
      {"inteiro", TOKEN_INTEIRO}, {"real", TOKEN_REAL},
      {"caractere", TOKEN_CARACTERE}, {"logico", TOKEN_LOGICO},
      {"vetor", TOKEN_VETOR}, {"de", TOKEN_DE},
      {"se", TOKEN_SE}, {"entao", TOKEN_ENTAO},
      {"senao", TOKEN_SENAO}, {"fimse", TOKEN_FIMSE},
      {"para", TOKEN_PARA}, {"ate", TOKEN_ATE},
      {"passo", TOKEN_PASSO}, {"faca", TOKEN_FACA},
      {"fimpara", TOKEN_FIMPARA}, {"enquanto", TOKEN_ENQUANTO},
      {"fimenquanto", TOKEN_FIMENQUANTO},
      {"procedimento", TOKEN_PROCEDIMENTO},
      {"fimprocedimento", TOKEN_FIMPROCEDIMENTO},
      {"funcao", TOKEN_FUNCAO}, {"fimfuncao", TOKEN_FIMFUNCAO},
      {"retorne", TOKEN_RETORNE}, {"leia", TOKEN_LEIA},
      {"escreva", TOKEN_ESCREVA}, {"escreval", TOKEN_ESCREVAL},
      {"verdadeiro", TOKEN_VERDADEIRO}, {"falso", TOKEN_FALSO},
      {"e", TOKEN_E}, {"ou", TOKEN_OU}, {"mod", TOKEN_MOD}
};

/* ============================================================
 *  2. ESTADO GLOBAL
 * ============================================================
 *  O fonte inteiro e carregado em memoria de uma vez e o lexico
 *  caminha sobre ele com um indice. Isso evita ficar voltando o
 *  ponteiro do arquivo quando e preciso olhar o proximo caractere
 *  (por exemplo para decidir entre "<", "<=", "<>" e "<-").
 * ============================================================ */

static char *buffer = NULL;         /* codigo fonte inteiro em memoria */
static int posicao = 0;             /* posicao atual no buffer         */
static int linhaAtual = 1;          /* linha atual do fonte            */
static FILE *arquivoSaida = NULL;   /* saida_tokens.txt                */

static Token lookahead;             /* token atual visto pelo parser   */

/* Tabela de simbolos simples: guarda os lexemas de identificadores e
 * de cadeias. O atributo table_index do token e o indice aqui dentro. */
static char tabelaSimbolos[MAX_SIMBOLOS][MAX_LEXEMA];
static int totalSimbolos = 0;

/* ============================================================
 *  3. FUNCOES AUXILIARES
 * ============================================================ */

/*
 * carregarArquivo
 * ------------------------------------------------------------
 * Le o arquivo fonte inteiro para o buffer global, terminando o
 * conteudo com '\0' para que o lexico possa detectar o fim sem
 * precisar guardar o tamanho.
 *
 * O arquivo e aberto em modo binario de proposito: em modo texto o
 * MinGW converte "\r\n" em "\n" e o tamanho devolvido por ftell deixa
 * de bater com o que foi lido. Lendo em binario, o '\r' sobra no
 * buffer e e descartado depois por pularEspacosEComentarios.
 *
 * Parametro: nomeArquivo - caminho do fonte .alg
 * Retorno:   1 em caso de sucesso, 0 se nao conseguiu abrir o arquivo
 *            ou alocar memoria.
 */
int carregarArquivo(const char *nomeArquivo)
{
      FILE *arquivo = fopen(nomeArquivo, "rb");
      if (arquivo == NULL)
            return 0;

      fseek(arquivo, 0, SEEK_END);
      long tamanho = ftell(arquivo);
      fseek(arquivo, 0, SEEK_SET);

      buffer = malloc(tamanho + 1);
      if (buffer == NULL)
      {
            fclose(arquivo);
            return 0;
      }

      fread(buffer, 1, tamanho, arquivo);
      buffer[tamanho] = '\0';
      fclose(arquivo);
      return 1;
}

/*
 * inserirSimbolo
 * ------------------------------------------------------------
 * Procura o lexema na tabela de simbolos e, se ainda nao existir,
 * insere no fim. A busca e linear porque a tabela e pequena e o
 * projeto nao exige desempenho nesta fase.
 *
 * Como o mesmo lexema sempre devolve o mesmo indice, duas ocorrencias
 * da variavel "idade" no fonte recebem o mesmo atributo na listagem.
 *
 * Parametro: lexema - texto ja terminado em '\0'
 * Retorno:   indice do lexema na tabela.
 */
int inserirSimbolo(const char *lexema)
{
      for (int i = 0; i < totalSimbolos; i++)
      {
            if (strcmp(tabelaSimbolos[i], lexema) == 0)
                  return i;
      }

      /* Tabela cheia: e melhor parar com uma mensagem clara do que
       * escrever fora do vetor e corromper a memoria. */
      if (totalSimbolos >= MAX_SIMBOLOS)
      {
            printf("ERRO: tabela de simbolos cheia (limite de %d entradas)\n",
                   MAX_SIMBOLOS);
            if (arquivoSaida != NULL)
            {
                  fprintf(arquivoSaida,
                          "ERRO: tabela de simbolos cheia (limite de %d entradas)\n",
                          MAX_SIMBOLOS);
                  fclose(arquivoSaida);
            }
            exit(1);
      }

      strncpy(tabelaSimbolos[totalSimbolos], lexema, MAX_LEXEMA - 1);
      tabelaSimbolos[totalSimbolos][MAX_LEXEMA - 1] = '\0';
      return totalSimbolos++;
}

/*
 * nomeDoToken
 * ------------------------------------------------------------
 * Traduz um valor do enum TokenNome para o texto que aparece na
 * listagem e nas mensagens de erro.
 *
 * Parametro: tipo - valor do enum
 * Retorno:   nome do token, ou "DESCONHECIDO" se o valor estiver fora
 *            da faixa valida (nao deve acontecer, e so uma protecao).
 */
const char *nomeDoToken(TokenNome tipo)
{
      if ((int)tipo < 0 || tipo >= TOKEN_TOTAL || nomesTokens[tipo] == NULL)
            return "DESCONHECIDO";
      return nomesTokens[tipo];
}

/*
 * nomeDoOpRel
 * ------------------------------------------------------------
 * Mesma ideia de nomeDoToken, mas para o sub-codigo guardado no
 * atributo dos operadores relacionais.
 *
 * Parametro: op - valor do enum OpRelType
 * Retorno:   nome do operador, ou "DESCONHECIDO" fora da faixa.
 */
const char *nomeDoOpRel(OpRelType op)
{
      if ((int)op < 0 || op >= OP_TOTAL || nomesOpRel[op] == NULL)
            return "DESCONHECIDO";
      return nomesOpRel[op];
}

/*
 * infoToken
 * ------------------------------------------------------------
 * Monta em 'destino' a linha de saida de um token, no formato pedido
 * pelo enunciado:
 *
 *    linha# NOME | atributo
 *
 * O atributo muda conforme o tipo do token: indice na tabela de
 * simbolos (ID e STRING), valor ja convertido (NUM_INT e NUM_FLOAT),
 * nome do operador (OP_REL) ou '-' quando o token nao tem atributo.
 *
 * O buffer vem do chamador em vez de ser um 'static' interno. Assim
 * duas chamadas dentro do mesmo printf nao sobrescrevem uma a outra,
 * desde que cada uma receba o seu proprio buffer.
 *
 * Parametros: token   - token a formatar
 *             destino - buffer de saida
 *             tamanho - capacidade de 'destino', em bytes
 * Retorno:    o proprio 'destino', para permitir uso direto dentro de
 *             um printf.
 */
char *infoToken(Token token, char *destino, size_t tamanho)
{
      if (destino == NULL || tamanho == 0)
            return destino;

      switch (token.type)
      {
            case TOKEN_ID:
            case TOKEN_STRING:
                  snprintf(destino, tamanho, "%d# %s | %d",
                           token.line, nomeDoToken(token.type),
                           token.attribute.table_index);
                  break;
            case TOKEN_NUM_INT:
                  snprintf(destino, tamanho, "%d# %s | %d",
                           token.line, nomeDoToken(token.type),
                           token.attribute.int_value);
                  break;
            case TOKEN_NUM_FLOAT:
                  snprintf(destino, tamanho, "%d# %s | %g",
                           token.line, nomeDoToken(token.type),
                           token.attribute.float_value);
                  break;
            case TOKEN_OP_REL:
                  snprintf(destino, tamanho, "%d# %s | %s",
                           token.line, nomeDoToken(token.type),
                           nomeDoOpRel(token.attribute.op_code));
                  break;
            default:
                  snprintf(destino, tamanho, "%d# %s | -",
                           token.line, nomeDoToken(token.type));
                  break;
      }

      return destino;
}

/*
 * imprimirToken
 * ------------------------------------------------------------
 * Escreve a linha formatada do token na tela e no arquivo de saida,
 * como pede a Etapa 2 (o mesmo resultado nos dois lugares).
 *
 * Parametro: token - token a registrar
 */
void imprimirToken(Token token)
{
      char linhaSaida[MAX_LINHA_SAIDA];
      infoToken(token, linhaSaida, sizeof(linhaSaida));
      printf("%s\n", linhaSaida);
      fprintf(arquivoSaida, "%s\n", linhaSaida);
}

/*
 * igualSemCaixa
 * ------------------------------------------------------------
 * Compara dois lexemas ignorando maiusculas e minusculas.
 *
 * Foi escrita a mao porque strcasecmp e uma extensao POSIX e o MinGW,
 * que e o compilador usado na correcao, nao garante a funcao.
 *
 * Parametros: a, b - cadeias terminadas em '\0'
 * Retorno:    1 quando sao iguais, 0 caso contrario.
 */
int igualSemCaixa(const char *a, const char *b)
{
      while (*a != '\0' && *b != '\0')
      {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
                  return 0;
            a++;
            b++;
      }
      return *a == *b;
}

/* ============================================================
 *  4. TRATAMENTO DE ERROS
 * ============================================================
 *  As duas funcoes seguem o que o enunciado pede: mostram a mensagem
 *  com a linha do fonte e encerram todo o processo. Antes do exit o
 *  arquivo de saida e fechado, senao os tokens ja escritos ficariam
 *  presos no buffer do stdio e o arquivo sairia vazio justamente nos
 *  testes de erro.
 * ============================================================ */

/*
 * erroSintatico
 * ------------------------------------------------------------
 * Relata um token que nao era o esperado pela gramatica e encerra o
 * programa com codigo 1.
 *
 * Parametros: token    - token encontrado (traz a linha do fonte)
 *             esperado - texto do que a gramatica esperava ali
 */
void erroSintatico(Token token, const char *esperado)
{
      printf("%d# ERRO SINTATICO: esperado %s, encontrado %s\n",
             token.line, esperado, nomeDoToken(token.type));
      fprintf(arquivoSaida, "%d# ERRO SINTATICO: esperado %s, encontrado %s\n",
              token.line, esperado, nomeDoToken(token.type));
      fclose(arquivoSaida);
      exit(1);
}

/*
 * erroLexico
 * ------------------------------------------------------------
 * Relata uma sequencia que nao forma nenhum lexema valido e encerra o
 * programa com codigo 1.
 *
 * Parametros: linha     - linha do fonte onde o erro foi localizado
 *             sequencia - trecho lexicamente errado
 */
void erroLexico(int linha, const char *sequencia)
{
      printf("%d# ERRO LEXICO: '%s'\n", linha, sequencia);
      fprintf(arquivoSaida, "%d# ERRO LEXICO: '%s'\n", linha, sequencia);
      fclose(arquivoSaida);
      exit(1);
}

/* ============================================================
 *  5. ANALISADOR LEXICO (SCANNER)  -  Etapa 2
 * ============================================================
 *  Expressoes regulares reconhecidas (ver etapa1_gramatica.txt):
 *
 *    ID        -> (letra | "_") (letra | digito | "_")*
 *    NUM_INT   -> digito+
 *    NUM_FLOAT -> digito+ "." digito+
 *    STRING    -> '"' (qualquer caractere menos '"' e quebra de linha)* '"'
 *    OP_REL    -> "<" | "<=" | "=" | ">" | ">=" | "<>"
 *    comentario-> "//" (qualquer caractere ate o fim da linha)
 *
 *  O enunciado garante que os lexemas vem separados por espaco. Como
 *  alguns exemplos do anexo aparecem sem espaco (escreval("...") por
 *  exemplo), o lexico foi feito para funcionar nos dois casos: ele
 *  sempre decide o fim do lexema pelo proximo caractere, e nao pelo
 *  espaco em branco.
 * ============================================================ */

/*
 * pularEspacosEComentarios
 * ------------------------------------------------------------
 * Avanca a posicao no buffer enquanto encontrar espacos, tabulacoes,
 * quebras de linha, o '\r' dos arquivos do Windows ou comentarios de
 * linha iniciados por "//".
 *
 * E aqui que linhaAtual e incrementada, entao o contador de linhas do
 * programa inteiro depende desta funcao ser chamada antes de comecar
 * a reconhecer cada token.
 */
void pularEspacosEComentarios(void)
{
      while (buffer[posicao] != '\0')
      {
            char c = buffer[posicao];

            if (c == '\n')
            {
                  linhaAtual++;
                  posicao++;
            }
            else if (isspace((unsigned char)c))
            {
                  posicao++;   /* espaco, tab, \r do Windows */
            }
            else if (c == '/' && buffer[posicao + 1] == '/')
            {
                  /* comentario: descarta tudo ate o fim da linha, sem
                   * consumir o '\n' (quem conta a linha e o caso acima) */
                  while (buffer[posicao] != '\n' && buffer[posicao] != '\0')
                        posicao++;
            }
            else
            {
                  break;
            }
      }
}

/*
 * reconhecerToken
 * ------------------------------------------------------------
 * Le o proximo lexema do buffer e devolve o token correspondente,
 * ja com o atributo preenchido. E a funcao que implementa de fato as
 * expressoes regulares da linguagem.
 *
 * A ordem dos testes e: fim de arquivo, numero, cadeia, identificador
 * ou palavra reservada e, por ultimo, os simbolos. Qualquer caractere
 * que nao se encaixe em nenhum desses casos vira erro lexico.
 *
 * E 'static' porque so obterToken deve chama-la: quem passa pelo resto
 * do programa tem que passar por obterToken, que tambem registra o
 * token na listagem.
 *
 * Retorno: o proximo token do fonte (TOKEN_EOF quando o fonte acaba).
 */
static Token reconhecerToken(void)
{
      Token token;

      pularEspacosEComentarios();

      /* a linha e fixada aqui, antes de consumir o lexema, para que um
       * lexema que termine no fim da linha ainda seja reportado na
       * linha em que comecou */
      token.line = linhaAtual;
      token.attribute.int_value = 0;

      /* --- fim do arquivo ------------------------------------- */
      if (buffer[posicao] == '\0')
      {
            token.type = TOKEN_EOF;
            return token;
      }

      /* --- numeros: NUM_INT e NUM_FLOAT ----------------------- */
      if (isdigit((unsigned char)buffer[posicao]))
      {
            char lexema[MAX_LEXEMA];
            int ehReal = 0;
            int inicio = posicao;

            while (isdigit((unsigned char)buffer[posicao]))
                  posicao++;

            if (buffer[posicao] == '.')
            {
                  if (isdigit((unsigned char)buffer[posicao + 1]))
                  {
                        /* 1.60 -> consome o ponto e a parte decimal */
                        ehReal = 1;
                        posicao++;
                        while (isdigit((unsigned char)buffer[posicao]))
                              posicao++;
                  }
                  else if (buffer[posicao + 1] != '.')
                  {
                        /* "1." sem digito depois e um real malformado.
                         * O caso "1..4" (faixa de vetor) cai fora deste
                         * if: o numero termina aqui e o ".." vira o
                         * proximo token. */
                        int parcial = posicao - inicio + 1;
                        if (parcial >= MAX_LEXEMA)
                              parcial = MAX_LEXEMA - 1;
                        memcpy(lexema, buffer + inicio, parcial);
                        lexema[parcial] = '\0';
                        erroLexico(token.line, lexema);
                  }
            }

            int tamanho = posicao - inicio;

            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erroLexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            if (ehReal)
            {
                  token.type = TOKEN_NUM_FLOAT;
                  token.attribute.float_value = strtod(lexema, NULL);
            }
            else
            {
                  token.type = TOKEN_NUM_INT;
                  token.attribute.int_value = (int)strtol(lexema, NULL, 10);
            }

            return token;
      }

      /* --- cadeias de caracteres ------------------------------ */
      if (buffer[posicao] == '"')
      {
            char lexema[MAX_LEXEMA];
            int inicio = posicao;

            posicao++;   /* passa da aspa de abertura */

            /* a quebra de linha encerra a busca: uma cadeia nao pode
             * atravessar linhas, entao "abc sem fechar vira erro na
             * propria linha em que comecou */
            while (buffer[posicao] != '"'
                   && buffer[posicao] != '\n'
                   && buffer[posicao] != '\0')
                  posicao++;

            if (buffer[posicao] != '"')
            {
                  /* cadeia aberta: mostra o trecho lido ate onde deu */
                  int parcial = posicao - inicio;
                  if (parcial >= MAX_LEXEMA)
                        parcial = MAX_LEXEMA - 1;
                  memcpy(lexema, buffer + inicio, parcial);
                  lexema[parcial] = '\0';
                  erroLexico(token.line, lexema);
            }

            posicao++;   /* consome a aspa de fechamento */

            int tamanho = posicao - inicio;

            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erroLexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            /* o lexema e guardado com as aspas, para que a cadeia
             * "idade" e a variavel idade nao ocupem o mesmo indice da
             * tabela de simbolos */
            token.type = TOKEN_STRING;
            token.attribute.table_index = inserirSimbolo(lexema);

            return token;
      }

      /* --- identificadores e palavras reservadas -------------- */
      if (isalpha((unsigned char)buffer[posicao]) || buffer[posicao] == '_')
      {
            char lexema[MAX_LEXEMA];
            int inicio = posicao;

            while (isalnum((unsigned char)buffer[posicao]) || buffer[posicao] == '_')
                  posicao++;

            int tamanho = posicao - inicio;

            /* um lexema maior que o limite vira erro em vez de ser
             * truncado: truncar faria dois identificadores longos e
             * diferentes virarem o mesmo simbolo */
            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erroLexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            /* assume identificador e rebaixa para palavra reservada se
             * o lexema estiver na tabela */
            token.type = TOKEN_ID;
            int n = sizeof(palavrasReservadas) / sizeof(palavrasReservadas[0]);
            for (int i = 0; i < n; i++)
            {
                  if (igualSemCaixa(lexema, palavrasReservadas[i].lexema))
                  {
                        token.type = palavrasReservadas[i].token;
                        break;
                  }
            }

            /* so identificador ocupa lugar na tabela de simbolos */
            if (token.type == TOKEN_ID)
                  token.attribute.table_index = inserirSimbolo(lexema);

            return token;
      }

      /* --- operadores e pontuacao ----------------------------- */
      switch (buffer[posicao])
      {
            case '+':
                  token.type = TOKEN_SOMA;
                  posicao++;
                  break;
            case '-':
                  token.type = TOKEN_SUB;
                  posicao++;
                  break;
            case '*':
                  token.type = TOKEN_MULT;
                  posicao++;
                  break;
            case '/':
                  /* o caso "//" ja foi consumido como comentario antes
                   * de chegar aqui, entao esta barra e sempre divisao */
                  token.type = TOKEN_DIV;
                  posicao++;
                  break;
            case '\\':
                  token.type = TOKEN_DIV_INT;
                  posicao++;
                  break;
            case '(':
                  token.type = TOKEN_ABRE_PAR;
                  posicao++;
                  break;
            case ')':
                  token.type = TOKEN_FECHA_PAR;
                  posicao++;
                  break;
            case '[':
                  token.type = TOKEN_ABRE_COL;
                  posicao++;
                  break;
            case ']':
                  token.type = TOKEN_FECHA_COL;
                  posicao++;
                  break;
            case ',':
                  token.type = TOKEN_VIRGULA;
                  posicao++;
                  break;
            case ':':
                  token.type = TOKEN_DOIS_PONTOS;
                  posicao++;
                  break;
            case '=':
                  /* no Visualg a igualdade e um "=" so */
                  token.type = TOKEN_OP_REL;
                  token.attribute.op_code = OP_EQ;
                  posicao++;
                  break;
            case '>':
                  /* ">" ou ">=": e preciso olhar o proximo caractere */
                  posicao++;                          /* consome o '>' */
                  token.type = TOKEN_OP_REL;
                  if (buffer[posicao] == '=')
                  {
                        token.attribute.op_code = OP_GE;
                        posicao++;                    /* consome o '=' */
                  }
                  else
                  {
                        token.attribute.op_code = OP_GT;
                  }
                  break;
            case '<':
                  /* o '<' e o caractere mais ambiguo da linguagem:
                   * pode abrir "<=", "<>" (relacionais) ou "<-"
                   * (atribuicao), e sozinho e o "menor que" */
                  posicao++;                          /* consome o '<' */
                  if (buffer[posicao] == '=')
                  {
                        token.type = TOKEN_OP_REL;
                        token.attribute.op_code = OP_LE;
                        posicao++;
                  }
                  else if (buffer[posicao] == '>')
                  {
                        token.type = TOKEN_OP_REL;
                        token.attribute.op_code = OP_NE;
                        posicao++;
                  }
                  else if (buffer[posicao] == '-')
                  {
                        token.type = TOKEN_ATRIB;
                        posicao++;
                  }
                  else
                  {
                        token.type = TOKEN_OP_REL;
                        token.attribute.op_code = OP_LT;
                  }
                  break;
            case '.':
                  /* so existe ".." na linguagem (faixa de vetor); um
                   * ponto sozinho nao forma lexema nenhum */
                  posicao++;                    /* consome o 1o ponto */
                  if (buffer[posicao] == '.')
                  {
                        token.type = TOKEN_PONTO_PONTO;
                        posicao++;              /* consome o 2o ponto */
                  }
                  else
                  {
                        char seq[2] = { '.', '\0' };
                        erroLexico(token.line, seq);
                  }
                  break;
            default:
            {
                  /* caractere que nao pertence ao alfabeto da linguagem */
                  char seq[2] = { buffer[posicao], '\0' };
                  erroLexico(token.line, seq);
            }
      }

      return token;
}

/*
 * obterToken
 * ------------------------------------------------------------
 * Interface do lexico para o parser (Figura 1 do enunciado).
 *
 * Pede o proximo token ao reconhecedor e registra a linha de saida
 * antes de devolve-lo. Por isso a listagem de tokens da Etapa 2 e
 * produzida numa unica passagem, junto com a analise sintatica da
 * Etapa 3, sem precisar ler o fonte duas vezes.
 *
 * Isso so e seguro porque a gramatica e LL(1): o parser nunca volta
 * atras, entao nenhum token e lido (nem impresso) duas vezes.
 *
 * Retorno: o proximo token do fonte.
 */
Token obterToken(void)
{
      Token token = reconhecerToken();
      imprimirToken(token);
      return token;
}

/* ============================================================
 *  6. ANALISADOR SINTATICO (PARSER)  -  Etapa 3
 * ============================================================
 *  Analise descendente recursiva: cada nao-terminal da gramatica vira
 *  uma funcao, e a escolha da producao e feita olhando um unico token
 *  a frente (o lookahead). A gramatica implementada e:
 *
 *    programa     -> ALGORITMO STRING declaracoes INICIO comandos
 *                    FIMALGORITMO
 *    declaracoes  -> { secao_var | procedimento | funcao }
 *    secao_var    -> VAR { decl_var }
 *    decl_var     -> lista_ids ":" tipo
 *    lista_ids    -> ID { "," ID }
 *    tipo         -> tipo_basico
 *                  | VETOR "[" NUM_INT ".." NUM_INT "]" DE tipo_basico
 *    tipo_basico  -> INTEIRO | REAL | CARACTERE | LOGICO
 *    procedimento -> PROCEDIMENTO ID [ "(" parametros ")" ]
 *                    INICIO comandos FIMPROCEDIMENTO
 *    funcao       -> FUNCAO ID "(" parametros ")" ":" tipo_basico
 *                    INICIO comandos FIMFUNCAO
 *    parametros   -> parametro { "," parametro }
 *    parametro    -> ID ":" tipo_basico
 *    comandos     -> { comando }
 *    comando      -> cmd_id | cmd_se | cmd_para | cmd_enquanto
 *                  | cmd_leia | cmd_escreva | cmd_retorne
 *    cmd_id       -> ID [ "<-" expr
 *                       | "[" expr "]" "<-" expr
 *                       | "(" argumentos ")" ]
 *    cmd_se       -> SE expr ENTAO comandos [ SENAO comandos ] FIMSE
 *    cmd_para     -> PARA ID DE expr ATE expr [ PASSO expr ] FACA
 *                    comandos FIMPARA
 *    cmd_enquanto -> ENQUANTO expr FACA comandos FIMENQUANTO
 *    cmd_leia     -> LEIA "(" variavel ")"
 *    cmd_escreva  -> ( ESCREVA | ESCREVAL ) "(" argumentos ")"
 *    cmd_retorne  -> RETORNE expr
 *    variavel     -> ID [ "[" expr "]" ]
 *    argumentos   -> expr { "," expr }
 *    expr         -> expr_e { OU expr_e }
 *    expr_e       -> expr_rel { E expr_rel }
 *    expr_rel     -> expr_arit [ OP_REL expr_arit ]
 *    expr_arit    -> termo { ( "+" | "-" ) termo }
 *    termo        -> fator { ( "*" | "/" | "\" | MOD ) fator }
 *    fator        -> NUM_INT | NUM_FLOAT | STRING | VERDADEIRO | FALSO
 *                  | "-" fator | "(" expr ")"
 *                  | ID [ "[" expr "]" | "(" argumentos ")" ]
 *
 *  A cadeia expr -> expr_e -> expr_rel -> expr_arit -> termo -> fator
 *  e o que da a precedencia dos operadores: OU e o mais fraco e os
 *  fatores sao os mais fortes.
 * ============================================================ */

/*
 * nextToken
 * ------------------------------------------------------------
 * Pede o proximo token ao lexico e guarda em lookahead. E a unica
 * forma de o parser avancar no fonte.
 */
void nextToken(void)
{
      lookahead = obterToken();
}

/*
 * consumir
 * ------------------------------------------------------------
 * Confere se o token atual e o esperado pela producao e avanca. Se
 * nao for, a analise para com erro sintatico.
 *
 * Parametro: esperado - token exigido pela gramatica neste ponto
 */
void consumir(TokenNome esperado)
{
      if (lookahead.type == esperado)
            nextToken();
      else
            erroSintatico(lookahead, nomeDoToken(esperado));
}

/* Prototipos: uma funcao por nao-terminal da gramatica. Sao declarados
 * aqui em cima porque as producoes se chamam em ciclo (expr chega em
 * fator, que volta em expr). */
void programa(void);
void declaracoes(void);
void secao_var(void);
void decl_var(void);
void lista_ids(void);
void tipo(void);
void tipo_basico(void);
void procedimento(void);
void funcao(void);
void parametros(void);
void parametro(void);
void comandos(void);
void comando(void);
void cmd_id(void);
void cmd_se(void);
void cmd_para(void);
void cmd_enquanto(void);
void cmd_leia(void);
void cmd_escreva(void);
void cmd_retorne(void);
void variavel(void);
void argumentos(void);
void expr(void);
void expr_e(void);
void expr_rel(void);
void expr_arit(void);
void termo(void);
void fator(void);

/*
 * argumentos -> expr { "," expr }
 * ------------------------------------------------------------
 * Lista de expressoes separadas por virgula, usada nas chamadas de
 * escreva, escreval, procedimentos e funcoes.
 */
void argumentos(void)
{
      expr();
      while (lookahead.type == TOKEN_VIRGULA)
      {
            consumir(TOKEN_VIRGULA);
            expr();
      }
}

/*
 * expr -> expr_e { OU expr_e }
 * ------------------------------------------------------------
 * Nivel mais externo da expressao: o operador logico OU, que tem a
 * menor precedencia da linguagem.
 */
void expr(void)
{
      expr_e();
      while (lookahead.type == TOKEN_OU)
      {
            consumir(TOKEN_OU);
            expr_e();
      }
}

/*
 * expr_e -> expr_rel { E expr_rel }
 * ------------------------------------------------------------
 * Operador logico E, que liga mais forte que o OU.
 */
void expr_e(void)
{
      expr_rel();
      while (lookahead.type == TOKEN_E)
      {
            consumir(TOKEN_E);
            expr_rel();
      }
}

/*
 * expr_rel -> expr_arit [ OP_REL expr_arit ]
 * ------------------------------------------------------------
 * Comparacao. E um 'if' e nao um 'while' porque a comparacao nao
 * encadeia: "a < b < c" nao faz sentido na linguagem.
 *
 * Qualquer operador relacional serve aqui, entao basta testar o tipo
 * TOKEN_OP_REL; qual deles era fica no atributo do token.
 */
void expr_rel(void)
{
      expr_arit();
      if (lookahead.type == TOKEN_OP_REL)
      {
            consumir(TOKEN_OP_REL);
            expr_arit();
      }
}

/*
 * expr_arit -> termo { ( "+" | "-" ) termo }
 * ------------------------------------------------------------
 * Soma e subtracao. Como os dois operadores levam ao mesmo lugar, o
 * avanco e feito direto com nextToken em vez de consumir.
 */
void expr_arit(void)
{
      termo();
      while (lookahead.type == TOKEN_SOMA || lookahead.type == TOKEN_SUB)
      {
            nextToken();
            termo();
      }
}

/*
 * termo -> fator { ( "*" | "/" | "\" | MOD ) fator }
 * ------------------------------------------------------------
 * Multiplicacao, divisao real, divisao inteira e resto: todos ligam
 * mais forte que soma e subtracao.
 */
void termo(void)
{
      fator();
      while (lookahead.type == TOKEN_MULT || lookahead.type == TOKEN_DIV
             || lookahead.type == TOKEN_DIV_INT || lookahead.type == TOKEN_MOD)
      {
            nextToken();
            fator();
      }
}

/*
 * fator -> NUM_INT | NUM_FLOAT | STRING | VERDADEIRO | FALSO
 *        | "-" fator | "(" expr ")"
 *        | ID [ "[" expr "]" | "(" argumentos ")" ]
 * ------------------------------------------------------------
 * Unidade indivisivel da expressao. Depois de um ID e preciso olhar o
 * proximo token para saber se e uma variavel simples, um elemento de
 * vetor ou uma chamada de funcao.
 *
 * O caso "-" fator e recursivo e cobre o menos unario, como em
 * "passo -2".
 */
void fator(void)
{
      switch (lookahead.type)
      {
            case TOKEN_NUM_INT:
            case TOKEN_NUM_FLOAT:
            case TOKEN_STRING:
            case TOKEN_VERDADEIRO:
            case TOKEN_FALSO:
                  nextToken();
                  break;
            case TOKEN_SUB:
                  consumir(TOKEN_SUB);
                  fator();
                  break;
            case TOKEN_ABRE_PAR:
                  consumir(TOKEN_ABRE_PAR);
                  expr();
                  consumir(TOKEN_FECHA_PAR);
                  break;
            case TOKEN_ID:
                  consumir(TOKEN_ID);
                  if (lookahead.type == TOKEN_ABRE_COL)
                  {
                        /* acesso a vetor: nomes[2] */
                        consumir(TOKEN_ABRE_COL);
                        expr();
                        consumir(TOKEN_FECHA_COL);
                  }
                  else if (lookahead.type == TOKEN_ABRE_PAR)
                  {
                        /* chamada de funcao: eh_par(num) */
                        consumir(TOKEN_ABRE_PAR);
                        argumentos();
                        consumir(TOKEN_FECHA_PAR);
                  }
                  break;
            default:
                  erroSintatico(lookahead, "expressao");
      }
}

/*
 * programa -> ALGORITMO STRING declaracoes INICIO comandos FIMALGORITMO
 * ------------------------------------------------------------
 * Simbolo inicial da gramatica. O nome do algoritmo vem entre aspas,
 * por isso o segundo token e STRING e nao ID.
 */
void programa(void)
{
      consumir(TOKEN_ALGORITMO);
      consumir(TOKEN_STRING);
      declaracoes();
      consumir(TOKEN_INICIO);
      comandos();
      consumir(TOKEN_FIMALGORITMO);
}

/*
 * declaracoes -> { secao_var | procedimento | funcao }
 * ------------------------------------------------------------
 * Aceita as tres formas em qualquer ordem e quantas vezes aparecerem,
 * ate encontrar um token que nao inicia nenhuma delas (normalmente o
 * INICIO do bloco principal).
 */
void declaracoes(void)
{
      while (1)
      {
            if (lookahead.type == TOKEN_VAR)
                  secao_var();
            else if (lookahead.type == TOKEN_PROCEDIMENTO)
                  procedimento();
            else if (lookahead.type == TOKEN_FUNCAO)
                  funcao();
            else
                  break;
      }
}

/*
 * secao_var -> VAR { decl_var }
 * ------------------------------------------------------------
 * A secao pode estar vazia (o anexo tem exemplos com "var" sem
 * nenhuma variavel embaixo), por isso o laco testa se ha um ID antes
 * de entrar.
 */
void secao_var(void)
{
      consumir(TOKEN_VAR);
      while (lookahead.type == TOKEN_ID)
            decl_var();
}

/*
 * decl_var -> lista_ids ":" tipo
 * ------------------------------------------------------------
 * Uma linha de declaracao, como "nome, sobrenome: caractere".
 */
void decl_var(void)
{
      lista_ids();
      consumir(TOKEN_DOIS_PONTOS);
      tipo();
}

/*
 * lista_ids -> ID { "," ID }
 * ------------------------------------------------------------
 * Um ou mais identificadores separados por virgula.
 */
void lista_ids(void)
{
      consumir(TOKEN_ID);
      while (lookahead.type == TOKEN_VIRGULA)
      {
            consumir(TOKEN_VIRGULA);
            consumir(TOKEN_ID);
      }
}

/*
 * tipo -> tipo_basico | VETOR "[" NUM_INT ".." NUM_INT "]" DE tipo_basico
 * ------------------------------------------------------------
 * Se comecar com VETOR, consome a faixa e o DE; nos dois casos o tipo
 * termina num tipo basico, entao a chamada final e comum as duas
 * producoes.
 */
void tipo(void)
{
      if (lookahead.type == TOKEN_VETOR)
      {
            consumir(TOKEN_VETOR);
            consumir(TOKEN_ABRE_COL);
            consumir(TOKEN_NUM_INT);
            consumir(TOKEN_PONTO_PONTO);
            consumir(TOKEN_NUM_INT);
            consumir(TOKEN_FECHA_COL);
            consumir(TOKEN_DE);
      }
      tipo_basico();
}

/*
 * tipo_basico -> INTEIRO | REAL | CARACTERE | LOGICO
 * ------------------------------------------------------------
 * Os quatro tipos primitivos do MiniVisualg.
 */
void tipo_basico(void)
{
      switch (lookahead.type)
      {
            case TOKEN_INTEIRO:
            case TOKEN_REAL:
            case TOKEN_CARACTERE:
            case TOKEN_LOGICO:
                  nextToken();
                  break;
            default:
                  erroSintatico(lookahead, "tipo (inteiro, real, caractere ou logico)");
      }
}

/*
 * iniciaComando
 * ------------------------------------------------------------
 * Conjunto FIRST de 'comando': diz se um token pode iniciar um
 * comando. E o que permite a comandos() saber onde a lista termina,
 * sem precisar conhecer o token que vem depois do bloco.
 *
 * Parametro: t - tipo do token a testar
 * Retorno:   1 se o token inicia um comando, 0 caso contrario.
 */
int iniciaComando(TokenNome t)
{
      return t == TOKEN_ID || t == TOKEN_SE || t == TOKEN_PARA
             || t == TOKEN_ENQUANTO || t == TOKEN_LEIA || t == TOKEN_ESCREVA
             || t == TOKEN_ESCREVAL || t == TOKEN_RETORNE;
}

/*
 * comandos -> { comando }
 * ------------------------------------------------------------
 * Sequencia de comandos, possivelmente vazia (um "se" sem nenhum
 * comando dentro e sintaticamente valido).
 */
void comandos(void)
{
      while (iniciaComando(lookahead.type))
            comando();
}

/*
 * comando -> cmd_id | cmd_se | cmd_para | cmd_enquanto
 *          | cmd_leia | cmd_escreva | cmd_retorne
 * ------------------------------------------------------------
 * Escolhe a producao pelo token atual. Cada comando tem um token
 * inicial proprio, por isso um unico lookahead basta.
 */
void comando(void)
{
      switch (lookahead.type)
      {
            case TOKEN_ID:       cmd_id();       break;
            case TOKEN_SE:       cmd_se();       break;
            case TOKEN_PARA:     cmd_para();     break;
            case TOKEN_ENQUANTO: cmd_enquanto(); break;
            case TOKEN_LEIA:     cmd_leia();     break;
            case TOKEN_ESCREVA:
            case TOKEN_ESCREVAL: cmd_escreva();  break;
            case TOKEN_RETORNE:  cmd_retorne();  break;
            default:             erroSintatico(lookahead, "comando");
      }
}

/*
 * cmd_escreva -> ( ESCREVA | ESCREVAL ) "(" argumentos ")"
 * ------------------------------------------------------------
 * Os dois comandos tem a mesma sintaxe e so diferem na quebra de
 * linha, que e questao de execucao e nao de analise. Por isso a
 * mesma funcao atende aos dois.
 */
void cmd_escreva(void)
{
      nextToken();   /* comando() ja conferiu que e ESCREVA ou ESCREVAL */
      consumir(TOKEN_ABRE_PAR);
      argumentos();
      consumir(TOKEN_FECHA_PAR);
}

/*
 * cmd_id   -> ID resto_id
 * resto_id -> "<-" expr | "[" expr "]" "<-" expr | "(" argumentos ")"
 *           | vazio
 * ------------------------------------------------------------
 * Um comando que comeca com identificador pode ser uma atribuicao,
 * uma atribuicao a elemento de vetor ou a chamada de um procedimento.
 * So da para decidir depois de consumir o ID e olhar o proximo token.
 *
 * O caso vazio cobre a chamada de procedimento sem parametros, que no
 * Visualg e escrita sem parenteses (linha_decorativa).
 */
void cmd_id(void)
{
      consumir(TOKEN_ID);
      if (lookahead.type == TOKEN_ATRIB)
      {
            consumir(TOKEN_ATRIB);
            expr();
      }
      else if (lookahead.type == TOKEN_ABRE_COL)
      {
            consumir(TOKEN_ABRE_COL);
            expr();
            consumir(TOKEN_FECHA_COL);
            consumir(TOKEN_ATRIB);
            expr();
      }
      else if (lookahead.type == TOKEN_ABRE_PAR)
      {
            consumir(TOKEN_ABRE_PAR);
            argumentos();
            consumir(TOKEN_FECHA_PAR);
      }
      /* senao: producao vazia, chamada de procedimento sem parametros */
}

/*
 * cmd_leia -> LEIA "(" variavel ")"
 * ------------------------------------------------------------
 * O argumento do leia tem de ser uma variavel (ou posicao de vetor),
 * nao uma expressao qualquer: ler para dentro de "a + b" nao faz
 * sentido. Por isso chama variavel() e nao expr().
 */
void cmd_leia(void)
{
      consumir(TOKEN_LEIA);
      consumir(TOKEN_ABRE_PAR);
      variavel();
      consumir(TOKEN_FECHA_PAR);
}

/*
 * variavel -> ID [ "[" expr "]" ]
 * ------------------------------------------------------------
 * Variavel simples ou elemento de vetor, como notas[i].
 */
void variavel(void)
{
      consumir(TOKEN_ID);
      if (lookahead.type == TOKEN_ABRE_COL)
      {
            consumir(TOKEN_ABRE_COL);
            expr();
            consumir(TOKEN_FECHA_COL);
      }
}

/*
 * cmd_se -> SE expr ENTAO comandos [ SENAO comandos ] FIMSE
 * ------------------------------------------------------------
 * O FIMSE obrigatorio elimina a ambiguidade do "senao pendurado": em
 * um se dentro de outro, cada senao pertence ao se que ainda nao foi
 * fechado.
 */
void cmd_se(void)
{
      consumir(TOKEN_SE);
      expr();
      consumir(TOKEN_ENTAO);
      comandos();
      if (lookahead.type == TOKEN_SENAO)
      {
            consumir(TOKEN_SENAO);
            comandos();
      }
      consumir(TOKEN_FIMSE);
}

/*
 * cmd_para -> PARA ID DE expr ATE expr [ PASSO expr ] FACA
 *             comandos FIMPARA
 * ------------------------------------------------------------
 * O PASSO e opcional; quando aparece, o valor pode ser negativo, como
 * em "para i de 10 ate 0 passo -2 faca" (o menos unario e tratado em
 * fator).
 */
void cmd_para(void)
{
      consumir(TOKEN_PARA);
      consumir(TOKEN_ID);
      consumir(TOKEN_DE);
      expr();
      consumir(TOKEN_ATE);
      expr();
      if (lookahead.type == TOKEN_PASSO)
      {
            consumir(TOKEN_PASSO);
            expr();
      }
      consumir(TOKEN_FACA);
      comandos();
      consumir(TOKEN_FIMPARA);
}

/*
 * cmd_enquanto -> ENQUANTO expr FACA comandos FIMENQUANTO
 * ------------------------------------------------------------
 * Laco condicional.
 */
void cmd_enquanto(void)
{
      consumir(TOKEN_ENQUANTO);
      expr();
      consumir(TOKEN_FACA);
      comandos();
      consumir(TOKEN_FIMENQUANTO);
}

/*
 * cmd_retorne -> RETORNE expr
 * ------------------------------------------------------------
 * Retorno de funcao. A gramatica nao exige que esteja dentro de uma
 * funcao: isso e verificacao semantica, fora do escopo da Fase 1.
 */
void cmd_retorne(void)
{
      consumir(TOKEN_RETORNE);
      expr();
}

/*
 * procedimento -> PROCEDIMENTO ID [ "(" parametros ")" ]
 *                 INICIO comandos FIMPROCEDIMENTO
 * ------------------------------------------------------------
 * A lista de parametros e opcional: o anexo diz que procedimentos sem
 * parametros nao levam parenteses nem na declaracao nem na chamada.
 */
void procedimento(void)
{
      consumir(TOKEN_PROCEDIMENTO);
      consumir(TOKEN_ID);
      if (lookahead.type == TOKEN_ABRE_PAR)
      {
            consumir(TOKEN_ABRE_PAR);
            parametros();
            consumir(TOKEN_FECHA_PAR);
      }
      consumir(TOKEN_INICIO);
      comandos();
      consumir(TOKEN_FIMPROCEDIMENTO);
}

/*
 * funcao -> FUNCAO ID "(" parametros ")" ":" tipo_basico
 *           INICIO comandos FIMFUNCAO
 * ------------------------------------------------------------
 * Diferente do procedimento, a funcao sempre declara parenteses e o
 * tipo de retorno depois dos dois pontos.
 */
void funcao(void)
{
      consumir(TOKEN_FUNCAO);
      consumir(TOKEN_ID);
      consumir(TOKEN_ABRE_PAR);
      parametros();
      consumir(TOKEN_FECHA_PAR);
      consumir(TOKEN_DOIS_PONTOS);
      tipo_basico();
      consumir(TOKEN_INICIO);
      comandos();
      consumir(TOKEN_FIMFUNCAO);
}

/*
 * parametros -> parametro { "," parametro }
 * ------------------------------------------------------------
 * Lista de parametros formais separados por virgula.
 */
void parametros(void)
{
      parametro();
      while (lookahead.type == TOKEN_VIRGULA)
      {
            consumir(TOKEN_VIRGULA);
            parametro();
      }
}

/*
 * parametro -> ID ":" tipo_basico
 * ------------------------------------------------------------
 * Um parametro formal, como "a: inteiro".
 */
void parametro(void)
{
      consumir(TOKEN_ID);
      consumir(TOKEN_DOIS_PONTOS);
      tipo_basico();
}

/* ============================================================
 *  7. MAIN
 * ============================================================ */

/*
 * main
 * ------------------------------------------------------------
 * Recebe o nome do fonte por linha de comando, carrega o arquivo,
 * abre a saida e dispara a analise.
 *
 * O primeiro nextToken() carrega o lookahead inicial (o parser sempre
 * precisa de um token na mao antes de comecar). Depois de programa(),
 * o teste de TOKEN_EOF garante que nao sobrou nada depois do
 * fimalgoritmo.
 *
 * Retorno: 0 se a analise terminou sem erros, 1 em caso de erro de
 *          uso ou de arquivo. Os erros lexicos e sintaticos encerram
 *          o programa antes, dentro das proprias funcoes de erro.
 */
int main(int argc, char *argv[])
{
      if (argc < 2)
      {
            printf("Uso: %s <arquivo_fonte.alg>\n", argv[0]);
            return 1;
      }

      if (!carregarArquivo(argv[1]))
      {
            printf("Erro ao abrir o arquivo %s\n", argv[1]);
            return 1;
      }

      arquivoSaida = fopen(ARQUIVO_SAIDA, "w");
      if (arquivoSaida == NULL)
      {
            printf("Erro ao criar %s\n", ARQUIVO_SAIDA);
            free(buffer);
            return 1;
      }

      /* Etapas 2 e 3 juntas: o parser pede os tokens ao lexico, que
       * imprime cada um deles enquanto a analise acontece. */
      nextToken();
      programa();

      if (lookahead.type != TOKEN_EOF)
            erroSintatico(lookahead, "fim do arquivo");

      printf("Analise sintatica concluida sem erros.\n");
      fprintf(arquivoSaida, "Analise sintatica concluida sem erros.\n");

      fclose(arquivoSaida);
      free(buffer);
      return 0;
}
