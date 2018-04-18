import random
random.seed(0)

cant_muestras = 10

with open("corpus", 'r') as f:
    lineas = f.readlines()
    # Saco el newline de cada linea
    # lineas = [i[:-1] for i in lineas]
   
    for j in range(0, 10):
        i = random.randint(0, len(lineas))
        a = random.sample(lineas, i)
        with open("casos_tests_corpus/corpus_%d" % j, 'w') as out:
            for l in a:
                out.write(l)
