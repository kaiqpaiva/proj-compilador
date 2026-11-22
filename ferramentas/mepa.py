#!/usr/bin/env python3
"""
mepa.py - Interpretador da MEPA (Maquina de Execucao Pascal)
=============================================================

Ferramenta de TESTE para o projeto de Compiladores (fase 2).
Nao faz parte do compilador: serve para executar o .txt gerado
e conferir se o resultado esta certo.

Le o formato dos slides da disciplina:
  - variaveis pelo NOME (CRVL a, ARMZ x) ou pelo ENDERECO (CRVL 0)
  - rotulos no formato "L1: NADA"
  - comentarios com ';' ou '//'
  - tambem aceita o formato do livro (CRVL 0,2) e o MULTI da VM do HugoJF
  - rotinas: CHPR L (chama), ENPR k (entra) e RTPR k,n (retorna), como no
    livro do Kowaltowski

Uso:
  python3 mepa.py programa.txt                 # executa
  python3 mepa.py programa.txt --trace         # mostra a pilha a cada instrucao
  python3 mepa.py programa.txt --entrada 4,7   # valores para os LEIT, sem perguntar

Os nomes das variaveis recebem enderecos na ordem em que aparecem
pela primeira vez no codigo (a, b, c... -> 0, 1, 2...).
"""

import argparse
import sys

# Quantas instrucoes executar antes de suspeitar de laco infinito.
LIMITE_PASSOS_PADRAO = 100_000

# Instrucoes que pegam os dois valores do topo e deixam um so.
# Cada uma recebe (M[s-1], M[s]) e devolve o resultado.
BINARIAS = {
    "SOMA": lambda a, b: a + b,
    "SUBT": lambda a, b: a - b,
    "MULT": lambda a, b: a * b,
    "MULTI": lambda a, b: a * b,          # grafia da VM do HugoJF
    "DIVI": None,                         # tratada a parte (divisao por zero)
    "DIVF": None,                         # tratada a parte (divisao por zero)
    "CONJ": lambda a, b: 1 if (a == 1 and b == 1) else 0,
    "DISJ": lambda a, b: 1 if (a == 1 or b == 1) else 0,
    "CMME": lambda a, b: 1 if a < b else 0,
    "CMMA": lambda a, b: 1 if a > b else 0,
    "CMIG": lambda a, b: 1 if a == b else 0,
    "CMDG": lambda a, b: 1 if a != b else 0,
    "CMEG": lambda a, b: 1 if a <= b else 0,
    "CMAG": lambda a, b: 1 if a >= b else 0,
}

SEM_OPERANDO = set(BINARIAS) | {"INPP", "PARA", "NADA", "INVR", "NEGA", "LEIT", "IMPR"}
COM_OPERANDO = {"AMEM", "DMEM", "CRCT", "CRVL", "ARMZ", "DSVS", "DSVF",
                "CHPR", "ENPR", "RTPR"}
DESVIOS = ("DSVS", "DSVF", "CHPR")       # instrucoes cujo operando e um rotulo


class ErroMEPA(Exception):
    """Erro de execucao ou de leitura, com a linha do arquivo."""

    def __init__(self, linha, mensagem):
        super().__init__(f"linha {linha}: {mensagem}")


class Instrucao:
    def __init__(self, nome, operando, linha, texto):
        self.nome = nome          # ex: "CRVL"
        self.operando = operando  # ex: "a", "3", "L1" ou None
        self.linha = linha        # linha no arquivo .txt
        self.texto = texto        # linha original, para o trace


def numero(texto):
    """Converte '3', '-2' ou '1.5' em numero. Devolve None se nao for."""
    try:
        return int(texto)
    except ValueError:
        try:
            return float(texto)
        except ValueError:
            return None


def montar(linhas):
    """
    Le o texto do programa e devolve (instrucoes, rotulos).
    rotulos: nome do rotulo -> indice da instrucao em P[].
    """
    instrucoes = []
    rotulos = {}
    rotulos_pendentes = []  # rotulo sozinho na linha ("L1:"), aponta para a proxima instrucao

    for num_linha, bruta in enumerate(linhas, start=1):
        texto = bruta.split(";")[0].split("//")[0].strip()
        if not texto:
            continue

        # Rotulo no comeco: "L1: NADA" ou "L1:"
        if ":" in texto:
            rotulo, _, resto = texto.partition(":")
            rotulo = rotulo.strip()
            if not rotulo or " " in rotulo:
                raise ErroMEPA(num_linha, f"rotulo invalido: '{texto}'")
            if rotulo in rotulos or rotulo in rotulos_pendentes:
                raise ErroMEPA(num_linha, f"rotulo '{rotulo}' definido duas vezes")
            rotulos_pendentes.append(rotulo)
            texto = resto.strip()
            if not texto:
                continue

        partes = texto.replace(",", " , ").split()
        nome = partes[0].upper()
        args = [p for p in partes[1:] if p != ","]

        if nome in COM_OPERANDO:
            if not args:
                raise ErroMEPA(num_linha, f"{nome} precisa de um operando")
            if nome == "RTPR":
                # RTPR k,n: nivel lexico e quantidade de parametros a descartar
                if len(args) != 2 or any(not isinstance(numero(a), int) for a in args):
                    raise ErroMEPA(num_linha, f"RTPR espera dois inteiros (ex: RTPR 1,0), recebeu '{texto}'")
                operando = (int(args[0]), int(args[1]))
            else:
                # Formato do livro: CRVL 0,2 -> nivel 0, endereco 2 (ignora o nivel)
                operando = args[-1] if nome in ("CRVL", "ARMZ") and len(args) == 2 else args[0]
                if len(args) > 2 or (len(args) == 2 and nome not in ("CRVL", "ARMZ")):
                    raise ErroMEPA(num_linha, f"operandos demais em '{texto}'")
        elif nome in SEM_OPERANDO:
            if args:
                raise ErroMEPA(num_linha, f"{nome} nao recebe operando ('{texto}')")
            operando = None
        else:
            raise ErroMEPA(num_linha, f"instrucao desconhecida: '{partes[0]}'")

        for r in rotulos_pendentes:
            rotulos[r] = len(instrucoes)
        rotulos_pendentes.clear()
        instrucoes.append(Instrucao(nome, operando, num_linha, texto))

    # Rotulo no fim do arquivo sem instrucao depois: aponta para o fim
    for r in rotulos_pendentes:
        rotulos[r] = len(instrucoes)

    # Confere se todo desvio aponta para um rotulo existente
    for inst in instrucoes:
        if inst.nome in DESVIOS and inst.operando not in rotulos:
            raise ErroMEPA(inst.linha, f"rotulo '{inst.operando}' nao definido")

    return instrucoes, rotulos


def nomes_de_variaveis(instrucoes):
    """Nomes usados em CRVL/ARMZ, na ordem em que aparecem pela primeira vez."""
    nomes = []
    for inst in instrucoes:
        if inst.nome in ("CRVL", "ARMZ") and numero(inst.operando) is None \
                and inst.operando not in nomes:
            nomes.append(inst.operando)
    return nomes


class MEPA:
    def __init__(self, instrucoes, rotulos, entradas=None, trace=False, limite=LIMITE_PASSOS_PADRAO):
        self.P = instrucoes
        self.rotulos = rotulos
        self.M = []               # pilha de dados; s = len(M) - 1
        self.i = 0                # contador de instrucao
        # nome da variavel -> endereco, na ordem da primeira aparicao no codigo
        self.enderecos = {nome: end for end, nome in enumerate(nomes_de_variaveis(instrucoes))}
        self.nomes = {end: nome for nome, end in self.enderecos.items()}  # para o trace
        self.total_vars = 0       # posicoes reservadas por AMEM
        self.D = {}               # registradores de base D[k] (rotinas)
        self.entradas = list(entradas) if entradas is not None else None
        self.trace = trace
        self.limite = limite
        self.saida = []

    # ---------- acesso a pilha ----------

    @property
    def s(self):
        return len(self.M) - 1

    def empilhar(self, valor):
        self.M.append(valor)

    def desempilhar(self, inst):
        if self.s < self.total_vars:
            raise ErroMEPA(inst.linha, f"{inst.nome}: pilha vazia (nao ha valor para desempilhar)")
        return self.M.pop()

    # ---------- variaveis ----------

    def endereco(self, inst):
        """Traduz o operando de CRVL/ARMZ (nome ou numero) para um endereco."""
        op = inst.operando
        n = numero(op)
        end = n if isinstance(n, int) else self.enderecos[op]
        if end < 0 or end >= self.total_vars:
            raise ErroMEPA(inst.linha,
                           f"{inst.nome} {op}: endereco {end} fora da area de variaveis "
                           f"(AMEM reservou {self.total_vars})")
        return end

    # ---------- execucao ----------

    def executar(self):
        passos = 0
        if self.trace:
            print(f"{'#':>4}  {'instrucao':<16} {'s':>3}  pilha / variaveis")
            print("-" * 72)

        while self.i < len(self.P):
            inst = self.P[self.i]
            self.i += 1
            passos += 1
            if passos > self.limite:
                raise ErroMEPA(inst.linha, f"mais de {self.limite} instrucoes executadas: "
                                           f"provavel laco infinito")

            if self.passo(inst):     # devolve True no PARA
                if self.trace:
                    self.mostrar(inst)
                return
            if self.trace:
                self.mostrar(inst)

        print("[aviso] o programa terminou sem executar PARA", file=sys.stderr)

    def passo(self, inst):
        nome, op = inst.nome, inst.operando

        if nome == "INPP":
            self.M = []
            self.total_vars = 0
        elif nome == "AMEM":
            n = numero(op)
            if not isinstance(n, int) or n < 0:
                raise ErroMEPA(inst.linha, f"AMEM espera um inteiro, recebeu '{op}'")
            self.M.extend([0] * n)
            self.total_vars += n
        elif nome == "DMEM":
            n = numero(op)
            if not isinstance(n, int) or n < 0 or n > self.total_vars:
                raise ErroMEPA(inst.linha, f"DMEM {op} invalido")
            del self.M[self.total_vars - n:self.total_vars]
            self.total_vars -= n
        elif nome == "PARA":
            return True
        elif nome == "NADA":
            pass
        elif nome == "CRCT":
            n = numero(op)
            if n is None:
                raise ErroMEPA(inst.linha, f"CRCT espera um numero, recebeu '{op}' "
                                           f"(variavel? use CRVL)")
            self.empilhar(n)
        elif nome == "CRVL":
            self.empilhar(self.M[self.endereco(inst)])
        elif nome == "ARMZ":
            end = self.endereco(inst)
            self.M[end] = self.desempilhar(inst)
        elif nome in BINARIAS:
            b = self.desempilhar(inst)
            a = self.desempilhar(inst)
            if nome in ("DIVI", "DIVF"):
                if b == 0:
                    raise ErroMEPA(inst.linha, f"{nome}: divisao por zero")
                resultado = int(a / b) if nome == "DIVI" else a / b  # DIVI trunca para zero, como em Pascal
            else:
                resultado = BINARIAS[nome](a, b)
            self.empilhar(resultado)
        elif nome == "INVR":
            self.empilhar(-self.desempilhar(inst))
        elif nome == "NEGA":
            self.empilhar(1 - self.desempilhar(inst))
        elif nome == "DSVS":
            self.i = self.rotulos[op]
        elif nome == "DSVF":
            if self.desempilhar(inst) == 0:
                self.i = self.rotulos[op]
        elif nome == "CHPR":
            # empilha o endereco de retorno (a instrucao seguinte) e desvia
            self.empilhar(self.i)
            self.i = self.rotulos[op]
        elif nome == "ENPR":
            k = numero(op)
            if not isinstance(k, int):
                raise ErroMEPA(inst.linha, f"ENPR espera um inteiro, recebeu '{op}'")
            self.empilhar(self.D.get(k, 0))      # salva o D[k] anterior
            self.D[k] = self.s + 1
        elif nome == "RTPR":
            k, n = op
            if self.s - self.total_vars < 1 + n:
                raise ErroMEPA(inst.linha, "RTPR: pilha sem o registro da rotina "
                                           "(RTPR fora de uma rotina chamada por CHPR?)")
            self.D[k] = self.M[self.s]           # restaura o D[k] salvo pelo ENPR
            self.i = self.M[self.s - 1]          # endereco de retorno do CHPR
            del self.M[self.s - 1 - n:]          # descarta retorno, D salvo e n parametros
        elif nome == "LEIT":
            self.empilhar(self.ler(inst))
        elif nome == "IMPR":
            valor = self.desempilhar(inst)
            texto = formatar(valor)
            self.saida.append(texto)
            print(f">> {texto}" if self.trace else texto)
        return False

    def ler(self, inst):
        if self.entradas is not None:
            if not self.entradas:
                raise ErroMEPA(inst.linha, "LEIT: acabaram os valores passados em --entrada")
            texto = self.entradas.pop(0)
        else:
            texto = input("LEIT> ")
        valor = numero(texto.strip())
        if valor is None:
            raise ErroMEPA(inst.linha, f"LEIT: '{texto}' nao e um numero")
        return valor

    def mostrar(self, inst):
        variaveis = " ".join(
            f"{self.nomes.get(e, '[' + str(e) + ']')}={formatar(self.M[e])}"
            for e in range(self.total_vars)
        )
        pilha = ", ".join(formatar(v) for v in self.M[self.total_vars:])
        print(f"{inst.linha:>4}  {inst.texto:<16} {self.s:>3}  [{pilha}]   {variaveis}")


def formatar(valor):
    if isinstance(valor, float) and valor.is_integer():
        return str(int(valor)) + ".0"
    if isinstance(valor, float):
        return f"{valor:.4f}".rstrip("0")
    return str(valor)


def main():
    parser = argparse.ArgumentParser(description="Interpretador da MEPA (ferramenta de teste)")
    parser.add_argument("arquivo", help="arquivo .txt com o codigo MEPA")
    parser.add_argument("--trace", action="store_true",
                        help="mostra a pilha e as variaveis depois de cada instrucao")
    parser.add_argument("--entrada", help="valores para os LEIT, separados por virgula (ex: 4,7)")
    parser.add_argument("--limite", type=int, default=LIMITE_PASSOS_PADRAO,
                        help="maximo de instrucoes antes de acusar laco infinito")
    args = parser.parse_args()

    try:
        with open(args.arquivo, encoding="utf-8") as f:
            instrucoes, rotulos = montar(f.readlines())
        entradas = args.entrada.split(",") if args.entrada else None
        MEPA(instrucoes, rotulos, entradas, args.trace, args.limite).executar()
    except FileNotFoundError:
        print(f"erro: arquivo '{args.arquivo}' nao encontrado", file=sys.stderr)
        sys.exit(2)
    except ErroMEPA as e:
        print(f"ERRO MEPA: {e}", file=sys.stderr)
        sys.exit(1)
    except BrokenPipeError:      # saida cortada por "| head", por exemplo
        sys.exit(0)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
