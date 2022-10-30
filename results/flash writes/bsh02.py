import xlrd
import xlwt

# 文件位置
excelfile = xlrd.open_workbook(r'C:\Users\Administrator\Desktop\result_bsh.xlsx')
# 获取目标EXCEL文件sheet名
print(excelfile.sheet_names())
workbook = xlwt.Workbook()
tmp1 = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't']
# 获取sheet内容【1.根据sheet索引2.根据sheet名称】
# sheet=ExcelFile.sheet_by_index(1)
for k in range(0, 20):
    sheet = excelfile.sheet_by_name(tmp1[k])
    worksheet = workbook.add_sheet(tmp1[k])
    m = sheet.row(0)[0].value
    for i in range(0, 48):
        t = sheet.row(i)[0].value
        for j in range(1, 12):
            if t > sheet.row(i)[j].value:
                t = sheet.row(i)[j].value
            if m > sheet.row(i)[j].value:
                m = sheet.row(i)[j].value
        worksheet.write(i, 12, t)
    worksheet.write(48, 12, m)
workbook.save('result.xlsx')
