# AWS Lambda Characteristics
memory_config = [0,256,512,1024,1792,2048,3008]
result = {
    'gzip_compression': [
        [4.63683939,2.223325729,1.090418339,0.617406607,0.613745928,0.624032974],
        [1187.030884,1138.342773,1116.588379,1106.392639,1256.95166,1877.091187],
        77
    ],
    'linpack': [
        [1.723002434,0.877117395,0.455329657,0.254618406,0.202791214,0.19717289],
        [441.088623,449.0841064,466.2575684,456.2761841,415.3164062,593.0960522],
        93
    ],
    'pyaes': [
        [6.856399536,3.586232901,1.617514133,0.964641809,0.927650928,0.927102089],
        [1755.238281,1836.151245,1656.334473,1728.638123,1899.829102,2788.723083],
        40
    ]
}

import matplotlib.pyplot as plt

parameters = {
    'axes.labelsize': 20, 
    'axes.titlesize': 20,
    'xtick.labelsize': 16,
    'ytick.labelsize': 16,
    'legend.fontsize': 16,
    'lines.linewidth': 5,
    'font.weight': 10,
    'figure.figsize': [6.5, 4.5],
    'font.sans-serif': 'Arial'
}

plt.rcParams.update(parameters)

for key in result.keys():
    plt.clf()
    
    x = [0, 1, 2, 3, 4, 5]
    
    fig, ax1 = plt.subplots()
    ax2 = ax1.twinx()

    ax1.plot(x, result[key][0], '-', label='Latency', marker='o', markersize=12, color='C0', alpha=0.7)
    ax2.plot(x, result[key][1], '--', label='Cost', marker='*', markersize=18, color='C1', alpha=0.7)
    
    ax1.set_xlabel('Memory (MB)')
    ax1.set_ylabel('Latency (s)')
    ax2.set_ylabel('Cost (MB*s)')
    ax1.legend(loc='upper left')
    ax2.legend(loc='upper right')
    
    ax1.set_xticklabels(memory_config)
    
    plt.tight_layout()
    plt.grid()
    plt.savefig(f'bg_{key}.pdf')
