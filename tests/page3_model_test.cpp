#include "page3model.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

QString serialize(const Page3Model& model)
{
    QString result;
    QXmlStreamWriter writer(&result);
    model.writeXml(writer);
    return result;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        Page3Model model;
        model.setInputTitles({"original"});
        model.setSelectedInputState(2, "original selection");
        model.setPage2LoadRowsChanged({{"load", {"1", "2"}}});
        const auto original = serialize(model);
        for (const QString& document : QStringList{
                 "<Other/>",
                 "<Page3><CurrentSelections><InputSelection index='9' text='changed'/></CurrentSelections><LoadRowsData>",
                 "<Page3><ComboBoxTitles><InputTitles><Title><invalid/></Title></InputTitles></ComboBoxTitles></Page3>"}) {
            QXmlStreamReader reader(document);
            reader.readNextStartElement();
            model.loadXml(reader);
            if (!reader.hasError() || serialize(model) != original)
                throw std::runtime_error("invalid Page3 XML modified live state");
        }
        QXmlStreamReader valid(QStringLiteral("<Page3><CurrentSelections><InputSelection index='4' text='new'/></CurrentSelections></Page3>"));
        valid.readNextStartElement();
        model.loadXml(valid);
        if (valid.hasError() || model.getSelectedInputIndex() != 4 || model.getInputTitles() != QStringList{"original"}
                || model.getLoadRowsData().size() != 1)
            throw std::runtime_error("partial Page3 document lost omitted state");
        std::cout << "PASS: Page3 snapshot commit and partial document compatibility\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
