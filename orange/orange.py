
# token type
INTEGER, PLUS, EOF = 'INTEGER', 'PLUS', 'EOF'

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
    def error(self):
        raise Exception('interpreter error!')
    
    def get_next_token(self):
        if self.pos > len(self.text) - 1:
            return Token(EOF, None)
        current_char = self.text[self.pos]
        digit_str = ''
        while True:
            if self.pos > len(self.text) - 1:
                if len(digit_str) >= 1:
                    return Token(INTEGER, int(digit_str))
                return Token(EOF, None)
            current_char = self.text[self.pos]
            if current_char.isdigit():
                self.pos += 1
                digit_str += current_char
                continue
            if current_char.isspace():
                self.pos += 1
                continue
            if len(digit_str) >= 1:
                token = Token(INTEGER, int(digit_str))
                return token
            else:
                break
        
        if current_char == '+':
            token = Token(PLUS, current_char)
            self.pos += 1
            return token
        self.error()
    def eat(self, token_type):
        """ 接受指定类型的token, 并且cur_token next
        
        """
        if self.current_token.type == token_type:
            self.current_token = self.get_next_token()
        else:
            self.error()
    def expr(self):
        """ 
        
        """
        self.current_token = self.get_next_token()
        left = self.current_token
        # expect a int
        self.eat(INTEGER)

        op = self.current_token
        self.eat(PLUS)

        right = self.current_token
        self.eat(INTEGER)

        result = left.value + right.value
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