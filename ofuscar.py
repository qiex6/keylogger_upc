def xor_ofuscar(texto, clave=0x55):
    resultado = []
    for c in texto:
        resultado.append(hex(ord(c) ^ clave))
    return ", ".join(resultado)

#aca reemplacen sus datos de tlgrm
token = "8545309765:AAHbs5t749K6RPYIQgreBJdIm49BN1xSZHQ"
chat_id = "6129038704"

print("Token ofuscado:")
print(xor_ofuscar(token))

print("\nChat ID ofuscado:")
print(xor_ofuscar(chat_id))