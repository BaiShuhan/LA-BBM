import xlrd
import xlwt

# 文件位置
excelfile = xlrd.open_workbook(r'C:\Users\Administrator\Desktop\RBER_result_blk_79.xlsx')
# 获取目标EXCEL文件sheet名
print(excelfile.sheet_names())
# 获取sheet内容【1.根据sheet索引2.根据sheet名称】
# sheet=ExcelFile.sheet_by_index(1)
sheet = excelfile.sheet_by_name('RBER_result_blk_79')
workbook = xlwt.Workbook()

tmp = [0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007, 0.008, 0.009, 0.01, 0.011, 0.012, 0.013, 0.014, 0.015,
       0.016, 0.017, 0.018, 0.019, 0.02]
tmp1 = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't']
for k in range(0, 20):
    print('----------------------------', tmp[k], '----------------------------------')
    worksheet = workbook.add_sheet(tmp1[k])
    for i in range(1, 577):
        for j in range(1, 363):
            value1 = sheet.row(j)[i].value  # 第j+1行第i+1列的值
            if value1 >= tmp[k]:
                if j+1 > 362:
                    t = j - 1
                    if i % 12 == 0:
                        m = i // 12 - 1
                    else:
                        m = i // 12
                    if i % 12 == 0:
                        n = 11
                    else:
                        n = i % 12 - 1
                    worksheet.write(m, n, sheet.row(t)[0].value / 100)
                    break
                else:
                    value2 = sheet.row(j+1)[i].value
                    if value2 >= tmp[k]:
                        t = j - 1
                        if i % 12 == 0:
                            m = i // 12 - 1
                        else:
                            m = i // 12
                        if i % 12 == 0:
                            n = 11
                        else:
                            n = i % 12 - 1
                        worksheet.write(m, n, sheet.row(t)[0].value/100)
                        break
            else:
                if j == 362:
                    if i % 12 == 0:
                        m = i // 12 - 1
                    else:
                        m = i // 12
                    if i % 12 == 0:
                        n = 11
                    else:
                        n = i % 12 - 1
                    worksheet.write(m, n, sheet.row(362)[0].value / 100)
workbook.save('result_bsh.xlsx')
