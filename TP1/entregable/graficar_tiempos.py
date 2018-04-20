import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

columnas = ["#Threads", "Tiempo(seg)", "Concurrente?"]
archivos_a_ver = [2, 5, 9]
a = pd.read_csv("results.csv")

for i in archivos_a_ver:
    d_archivo = a[a.ca == i]
    datos = pd.DataFrame(columns=columnas)
    for j in d_archivo.ct.unique():
        d_thread = d_archivo[d_archivo.ct == j]
        for k in d_thread.tiempo:
            datos = datos.append(pd.DataFrame([[j, k, False]], columns=columnas))
        for k in d_thread.tiempo_c:
            datos = datos.append(pd.DataFrame([[j, k, True]], columns=columnas))
    sns.boxplot(x="#Threads", y="Tiempo(seg)", hue="Concurrente?", data=datos, palette="Set1")
    plt.title("Tiempo vs #threads para %d archivos" % i)
    plt.show()
