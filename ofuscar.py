def xor_ofuscar(texto, clave=0x55):
    resultado = []
    for c in texto:
        resultado.append(hex(ord(c) ^ clave))
    return ", ".join(resultado)

#aca reemplacen sus datos de tlgrm
token = "42"
chat_id = "1234"

print("Token ofuscado:")
print(xor_ofuscar(token))

print("\nChat ID ofuscado:")
print(xor_ofuscar(chat_id))