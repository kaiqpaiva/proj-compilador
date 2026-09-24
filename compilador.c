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
 *  Saida: tokens na tela e no arquivo saida_tokens.txt
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ARQUIVO_SAIDA   "saida_tokens.txt"
#define MAX_LEXEMA      256
#define MAX_SIMBOLOS    512
#define MAX_LINHA_SAIDA 64      /* tamanho da linha formatada de um token */

/* ============================================================
 *  1. DEFINICAO DOS TOKENS (Figura 2 do enunciado + extras)
 * ============================================================ */

/* Nomes dos tokens usados pelo parser.
 * Os 6 primeiros sao os do enunciado. O resto foi adicionado porque
 * a linguagem tem simbolos e palavras reservadas que o parser precisa
 * diferenciar (ver readme.txt, decisoes de design). */
typedef enum {
    TOKEN_EOF = 0,
    TOKEN_ID,           /* identificadores (variaveis, funcoes)  */
    TOKEN_NUM_INT,      /* numeros inteiros (ex: 42)             */
    TOKEN_NUM_FLOAT,    /* numeros reais (ex: 3.14)              */
    TOKEN_OP_REL,       /* operadores relacionais                */
    TOKEN_KEYWORD,      /* mantido do enunciado (nao usado)      */

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

    TOKEN_TOTAL
} TokenNome;

/* Nome textual de cada token, usado na saida e nas mensagens de erro.
 * Indexada pelo proprio enum.    */
static const char *nomesTokens[] = {
    [TOKEN_EOF]             = "EOF",
    [TOKEN_ID]              = "ID",
    [TOKEN_NUM_INT]         = "NUM_INT",
    [TOKEN_NUM_FLOAT]       = "NUM_FLOAT",
    [TOKEN_OP_REL]          = "OP_REL",
    [TOKEN_KEYWORD]         = "KEYWORD",
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

/* Se este assert quebrar, faltou (ou sobrou) um nome na tabela acima. */
_Static_assert(sizeof(nomesTokens) / sizeof(nomesTokens[0]) == TOKEN_TOTAL,
               "nomesTokens dessincronizado com o enum TokenNome");

/* Sub-codigos do atributo dos operadores relacionais.
 * OP_NE foi adicionado para o <> que aparece no anexo. */
typedef enum {
    OP_LT,  /* <  */
    OP_LE,  /* <= */
    OP_EQ,  /* =  (no Visualg igualdade e um "=" so) */
    OP_GT,  /* >  */
    OP_GE,  /* >= */
    OP_NE,   /* <> */
    OP_TOTAL
} OpRelType;

/* Nome textual de cada sub-codigo de operador relacional.
 * Indexada pelo proprio enum.    */
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

/* Estrutura do token com a union de atributos (Figura 2) */
typedef struct {
    TokenNome type;         /* nome do token             */
    int line;               /* linha, para erros         */
    union {
        int table_index;    /* indice na tabela de simbolos (ID, STRING) */
        int int_value;      /* valor literal convertido  */
        double float_value; /* valor literal convertido  */
        OpRelType op_code;  /* operador relacional       */
    } attribute;
} Token;

/* Tabela de palavras reservadas: lexema -> token */
typedef struct {
    const char *lexema;
    TokenNome token;
} PalavraReservada;

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
 * ============================================================ */

static char *buffer = NULL;         /* codigo fonte inteiro em memoria */
static int posicao = 0;             /* posicao atual no buffer         */
static int linhaAtual = 1;          /* linha atual do fonte            */
static FILE *arquivoSaida = NULL;   /* saida_tokens.txt                */

static Token lookahead;             /* token atual visto pelo parser   */

/* Tabela de simbolos simples: guarda lexemas de IDs e strings */
static char tabelaSimbolos[MAX_SIMBOLOS][MAX_LEXEMA];
static int totalSimbolos = 0;

/* ============================================================
 *  3. FUNCOES AUXILIARES
 * ============================================================ */

/* Le o arquivo inteiro para o buffer. Retorna 0 se deu erro. */
int carregarArquivo(const char *nomeArquivo)
{
    FILE *arquivo = fopen(nomeArquivo, "rb");
    if (arquivo == NULL)
        return 0;

    fseek(arquivo, 0, SEEK_END);
    long tamanho = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);

    buffer = malloc(tamanho + 1);
    if (buffer == NULL) {
        fclose(arquivo);
        return 0;
    }
    fread(buffer, 1, tamanho, arquivo);
    buffer[tamanho] = '\0';
    fclose(arquivo);
    return 1;
}

/* Procura o lexema na tabela; se nao achar, insere. Retorna o indice. */
int inserirSimbolo(const char *lexema)
{
    for (int i = 0; i < totalSimbolos; i++) {
        if (strcmp(tabelaSimbolos[i], lexema) == 0)
            return i;
    }
    /* TODO: tratar tabela cheia */
    strncpy(tabelaSimbolos[totalSimbolos], lexema, MAX_LEXEMA - 1);
    return totalSimbolos++;
}

/* Converte o nome do token para texto (usado no print) */
const char *nomeDoToken(TokenNome tipo)
{
      if (tipo < 0 || tipo >= TOKEN_TOTAL || nomesTokens[tipo] == NULL)
            return "DESCONHECIDO";
      return nomesTokens[tipo];
}

/* Converte o sub-codigo do operador relacional para texto */
const char *nomeDoOpRel(OpRelType op)
{
      if (op < 0 || op >= OP_TOTAL || nomesOpRel[op] == NULL)
            return "DESCONHECIDO";
      return nomesOpRel[op];
}

/* Monta em 'destino' a linha de saida de um token, no formato
 *   linha# NOME | atributo
 * O atributo varia com o tipo: indice na tabela de simbolos (ID e
 * STRING), valor convertido (NUM_INT e NUM_FLOAT), nome do operador
 * (OP_REL), ou '-' quando o token nao tem atributo.
 * Devolve 'destino' para permitir uso direto dentro de um printf. */
char* infoToken(Token token, char *destino, size_t tamanho)
{
      if (destino == NULL || tamanho == 0)
            return destino;

      switch (token.type)
      {
            case TOKEN_ID:
            case TOKEN_STRING:
                  snprintf(destino, tamanho, "%d# %s | %d",
                        token.line, nomeDoToken(token.type), token.attribute.table_index);
                  break;
            case TOKEN_NUM_INT:
                  snprintf(destino, tamanho, "%d# %s | %d",
                        token.line, nomeDoToken(token.type), token.attribute.int_value);
                  break;
            case TOKEN_NUM_FLOAT:
                  snprintf(destino, tamanho, "%d# %s | %g",
                        token.line, nomeDoToken(token.type), token.attribute.float_value);
                  break;
            case TOKEN_OP_REL:
                  snprintf(destino, tamanho, "%d# %s | %s",
                        token.line, nomeDoToken(token.type), nomeDoOpRel(token.attribute.op_code));
                  break;
            default:
                  snprintf(destino, tamanho, "%d# %s | -",
                        token.line, nomeDoToken(token.type));
                  break;
      }

      return destino;
}

/* Imprime o token na tela e no arquivo no formato:
 *   linha# NOME | atributo                                  */
void imprimirToken(Token token)
{
      char linhaSaida[MAX_LINHA_SAIDA];
      infoToken(token, linhaSaida, sizeof(linhaSaida));
      printf("%s\n", linhaSaida);
      fprintf(arquivoSaida, "%s\n", linhaSaida);
}

/* Compara dois lexemas ignorando maiusculas e minusculas.
 * Devolve 1 quando sao iguais, 0 caso contrario.
 * Escrita a mao porque strcasecmp e POSIX e o MinGW nao garante. */
int igualSemCaixa(const char *a, const char *b)
{
      while (*a != '\0' && *b != '\0') {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
                  return 0;
            a++;
            b++;
      }
      return *a == *b;
}

/* ============================================================
 *  4. TRATAMENTO DE ERROS
 * ============================================================ */

void erroLexico(int linha, const char *sequencia)
{
    printf("%d# ERRO LEXICO: '%s'\n", linha, sequencia);
    fprintf(arquivoSaida, "%d# ERRO LEXICO: '%s'\n", linha, sequencia);
    fclose(arquivoSaida);
    exit(1);
}

void erroSintatico(Token token)
{
    printf("%d# ERRO SINTATICO: token inesperado %s\n",
           token.line, nomeDoToken(token.type));
    exit(1);
}

/* ============================================================
 *  5. ANALISADOR LEXICO (SCANNER)  -  Etapa 2
 * ============================================================ */

/* Pula espacos, quebras de linha e comentarios // ate o fim da linha */
void pularEspacosEComentarios(void)
{
    while (buffer[posicao] != '\0') {
        char c = buffer[posicao];

        if (c == '\n') {
            linhaAtual++;
            posicao++;
        } else if (isspace((unsigned char)c)) {
            posicao++;   /* espaco, tab, \r do Windows */
        } else if (c == '/' && buffer[posicao + 1] == '/') {
            while (buffer[posicao] != '\n' && buffer[posicao] != '\0')
                posicao++;
        } else {
            break;
        }
    }
}

/* Reconhece e devolve o proximo token do buffer */
static Token reconhecerToken(void)
{
      Token token;
      pularEspacosEComentarios();
      token.line = linhaAtual;
      token.attribute.int_value = 0;

      if (buffer[posicao] == '\0')
      {
            token.type = TOKEN_EOF;
            return token;
      }

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
                        /* 1. sem digito depois: real malformado */
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

      if (buffer[posicao] == '"')
      {
            char lexema[MAX_LEXEMA];

            int inicio = posicao;
            posicao++; /* passa da aspa de abertura */

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

            posicao++; /* consome a aspa de fechamento */

            int tamanho = posicao - inicio;

            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erroLexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            token.type = TOKEN_STRING;
            token.attribute.table_index = inserirSimbolo(lexema);

            return token;
      }

      if (isalpha((unsigned char) buffer[posicao]) || buffer[posicao] == '_')
      {
            char lexema[MAX_LEXEMA];

            int inicio = posicao;
            while (isalnum((unsigned char)buffer[posicao]) || buffer[posicao] == '_')
                  posicao++;

            int tamanho = posicao - inicio;

            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erroLexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

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

            if (token.type == TOKEN_ID)
                  token.attribute.table_index = inserirSimbolo(lexema);

            return token;
      }

      switch(buffer[posicao])
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
                  token.type = TOKEN_OP_REL;
                  token.attribute.op_code = OP_EQ;
                  posicao++;
                  break;
            case '>':
                  posicao++;                          /* consome o '>' */
                  token.type = TOKEN_OP_REL;
                  if (buffer[posicao] == '=')
                  {
                        token.attribute.op_code = OP_GE;
                        posicao++;                    /* consome o '=' */
                  }
                  else
                        token.attribute.op_code = OP_GT;
                  break;
            case '<':
                  posicao++;
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
                  posicao++;                          /* consome o primeiro ponto */
                  if (buffer[posicao] == '.')
                  {
                        token.type = TOKEN_PONTO_PONTO;
                        posicao++;                    /* consome o segundo ponto  */
                  }
                  else
                  {
                        char seq[2] = { '.', '\0' };
                        erroLexico(token.line, seq);
                  }
                  break;
            default: {
                  char seq[2] = { buffer[posicao], '\0' };
                  erroLexico(token.line, seq);
            }
      }

      return token;
}

/* Interface do lexico para o parser (Figura 1 do enunciado).
 * Pede o proximo token ao reconhecedor e registra a saida antes de
 * devolve-lo, de modo que a listagem de tokens seja produzida numa
 * unica passagem, tanto na Etapa 2 quanto durante a analise sintatica. */
Token obterToken(void)
{
      Token token = reconhecerToken();
      imprimirToken(token);
      return token;
}

/* ============================================================
 *  6. ANALISADOR SINTATICO (PARSER)  -  Etapa 3
 * ============================================================ */

/* Pede o proximo token ao lexico e guarda em lookahead */
void nextToken(void)
{
    lookahead = obterToken();
}

/* Confere se o token atual e o esperado e avanca */
void consumir(TokenNome esperado)
{
    if (lookahead.type == esperado)
        nextToken();
    else
        erroSintatico(lookahead);
}

/* TODO Etapa 3: uma funcao por nao-terminal da gramatica, ex:
 *   void programa(void);
 *   void declaracoes(void);
 *   void comando(void);
 *   void expressao(void);
 */

/* ============================================================
 *  7. MAIN
 * ============================================================ */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Uso: %s <arquivo_fonte.alg>\n", argv[0]);
        return 1;
    }

    if (!carregarArquivo(argv[1])) {
        printf("Erro ao abrir o arquivo %s\n", argv[1]);
        return 1;
    }

    arquivoSaida = fopen(ARQUIVO_SAIDA, "w");
    if (arquivoSaida == NULL) {
        printf("Erro ao criar %s\n", ARQUIVO_SAIDA);
        free(buffer);
        return 1;
    }

    /* Etapa 2: lista todos os tokens */
    do {
        nextToken();
    } while (lookahead.type != TOKEN_EOF);

    /* Etapa 3 (depois): voltar ao inicio e chamar o parser
     *   posicao = 0; linhaAtual = 1;
     *   nextToken();
     *   programa();
     *   consumir(TOKEN_EOF);
     *   printf("Analise sintatica concluida sem erros.\n");
     */

    fclose(arquivoSaida);
    free(buffer);
    return 0;
}
