import string

class RichTemplate(string.Template):
    delimiter = '$$'
