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
    TOKEN_E, TOKEN_OU, TOKEN_MOD
} TokenNome;

/* Sub-codigos do atributo dos operadores relacionais.
 * OP_NE foi adicionado para o <> que aparece no anexo. */
typedef enum {
    OP_LT,  /* <  */
    OP_LE,  /* <= */
    OP_EQ,  /* =  (no Visualg igualdade e um "=" so) */
    OP_GT,  /* >  */
    OP_GE,  /* >= */
    OP_NE   /* <> */
} OpRelType;

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
    /* TODO: completar com todos os tokens */
    switch (tipo) {
        case TOKEN_EOF:       return "EOF";
        case TOKEN_ID:        return "ID";
        case TOKEN_NUM_INT:   return "NUM_INT";
        case TOKEN_NUM_FLOAT: return "NUM_FLOAT";
        case TOKEN_OP_REL:    return "OP_REL";
        case TOKEN_STRING:    return "STRING";
        default:              return "TOKEN";
    }
}

/* Imprime o token na tela e no arquivo no formato:
 *   linha# NOME | atributo                                  */
void imprimirToken(Token token)
{
    /* TODO: imprimir o atributo certo conforme o tipo do token */
    printf("%d# %s\n", token.line, nomeDoToken(token.type));
    fprintf(arquivoSaida, "%d# %s\n", token.line, nomeDoToken(token.type));
}

/* ============================================================
 *  4. TRATAMENTO DE ERROS
 * ============================================================ */

void erroLexico(int linha, const char *sequencia)
{
    printf("%d# ERRO LEXICO: '%s'\n", linha, sequencia);
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
Token obterToken(void)
{
    Token token;
    pularEspacosEComentarios();
    token.line = linhaAtual;
    token.attribute.int_value = 0;

    if (buffer[posicao] == '\0') {
        token.type = TOKEN_EOF;
        return token;
    }

    /* TODO Etapa 2:
     *  - letra ou _  -> identificador ou palavra reservada
     *                   (comparar sem diferenciar maiuscula, ex: MOD, E)
     *  - digito      -> NUM_INT ou NUM_FLOAT
     *  - "           -> STRING ate a proxima aspa
     *  - < > =       -> OP_REL ou ATRIB (<-)
     *  - . . -> PONTO_PONTO
     *  - demais simbolos: + - * / \ ( ) [ ] , :
     *  - qualquer outra coisa -> erroLexico()
     */
    token.type = TOKEN_EOF;   /* provisorio ate implementar */
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
        imprimirToken(lookahead);
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
