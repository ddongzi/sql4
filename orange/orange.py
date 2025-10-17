
# token type
INTEGER, PLUS, MINUS, EOF = 'INTEGER', 'PLUS', 'MINUS', 'EOF'

class Token():
    def __init__(self, type, value):
        # token types : INTEGER, EOF
        self.type = type
        # token value: None
        self.value = value
    def __str__(self):
        return f'Token {self.type}, {repr(self.value)}'
    def __repr__(self):
        return self.__str__()

class Interpreter():
    def __init__(self, text):
        # input string
        self.text = text
        # pos on text
        self.pos = 0
        # token
        self.current_token = None
        self.current_char = self.text[self.pos]
    def error(self):
        raise Exception('interpreter error!')
    def advance(self):
        self.pos += 1
        if self.pos > len(self.text) - 1:
            self.current_char = None
        else:
            self.current_char = self.text[self.pos]
    def skip_space(self):
        while self.current_char is not None and self.current_char.isspace():
            self.advance()
    def integer(self):
        result = ''
        while self.current_char is not None and self.current_char.isdigit():
            result += self.current_char
            self.advance()
        return int(result)
    def get_next_token(self):
        while self.current_char is not None:
            if self.current_char.isspace():
                self.skip_space()
                continue
            if self.current_char.isdigit():
                return Token(INTEGER, self.integer())
            if self.current_char == '+':
                self.advance()
                return Token(PLUS, '+')
            if self.current_char == '-':
                self.advance()
                return Token(MINUS, '-')
            self.error()
        return Token(EOF, None)
    def eat(self, token_type):
        """ 接受指定类型的token, 并且cur_token next
        
        """
        if self.current_token.type == token_type:
            self.current_token = self.get_next_token()
        else:
            self.error()
    def term(self):
        """ 参考语法图
        
        """
        token = self.current_token
        self.eat(INTEGER)
        return token.value
    def expr(self):
        """ 解释表达式
        
        """
        self.current_token = self.get_next_token()
        result = self.term()
        # 还没开始 或者 没到末尾
        while self.current_token.type in (PLUS, MINUS):
            # expect a int
            op = self.current_token
            if op.type == PLUS:
                self.eat(PLUS)
                result = result + self.term()
            elif op.type == MINUS:
                self.eat(MINUS)
                result = result - self.term()
        return result
def main():
    while True:
        try:
            text = input('CALC> ')
        except EOFError:
            break
        if not text:
            continue
        interpreter = Interpreter(text=text)
        result = interpreter.expr()
        print(result)
if __name__ == '__main__':
    main()