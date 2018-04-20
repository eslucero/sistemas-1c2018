import os

numThreads = [2, 4, 16, 96]

def correr(ct, ca):
	path = './main %d %d >> results.csv' % (ct, ca)
	if(os.system(path) == -1):
		print('ERROR EN EL ARCHIVO %s' % path)


if not os.path.exists("results.csv"):
	with open("results.csv", 'w') as f:
		f.write("ct,ca,tiempo,tiempo_c\n")

for f in range(1, 10):
	for t in [i for i in numThreads if i <= f]:
		correr(t, f)
