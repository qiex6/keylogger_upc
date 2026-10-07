def xor_ofuscar(texto, clave=0x55):
    resultado = []
    for c in texto:
        resultado.append(hex(ord(c) ^ clave))
    return ", ".join(resultado)

#aca reemplacen sus datos de tlgrm
token = "asdas"
chat_id = "12345"

print("Token ofuscado:")
print(xor_ofuscar(token))

print("\nChat ID ofuscado:")
print(xor_ofuscar(chat_id))
