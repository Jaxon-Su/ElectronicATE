#include "manualcontrolmodel.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

QString serialize(const ManualControlModel& model)
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
        ManualControlModel model;
        model.setInputTitles({"original"});
        model.setDcSelection(0, 1, "main");
        model.setSelectedInputState(2, "original selection");
        model.setTestConditionsLoadRowsChanged({{"load", {"1", "2"}}});
        const auto original = serialize(model);
        ManualControlModel restored;
        QXmlStreamReader roundTrip(original);
        roundTrip.readNextStartElement();
        restored.loadXml(roundTrip);
        if (roundTrip.hasError() || restored.dcSelection(0).index != 1)
            throw std::runtime_error("DC selections did not round trip");
        for (const QString& document : QStringList{
                 "<Other/>",
                 "<ManualControl schemaVersion='2'><CurrentSelections><InputSelection index='9' text='changed'/></CurrentSelections><LoadRowsData>",
                 "<ManualControl schemaVersion='2'><ComboBoxTitles><InputTitles><Title><invalid/></Title></InputTitles></ComboBoxTitles></ManualControl>"}) {
            QXmlStreamReader reader(document);
            reader.readNextStartElement();
            model.loadXml(reader);
            if (!reader.hasError() || serialize(model) != original)
                throw std::runtime_error("invalid ManualControl XML modified live state");
        }
        QXmlStreamReader incomplete(QStringLiteral("<ManualControl schemaVersion='2'><CurrentSelections><InputSelection index='4' text='new'/></CurrentSelections></ManualControl>"));
        incomplete.readNextStartElement();
        model.loadXml(incomplete);
        if (!incomplete.hasError() || serialize(model) != original)
            throw std::runtime_error("incomplete ManualControl XML accepted or changed live state");
        QXmlStreamReader old(QStringLiteral("<ManualControl><CurrentSelections/></ManualControl>"));
        old.readNextStartElement(); model.loadXml(old);
        if (!old.hasError()) throw std::runtime_error("Old ManualControl format accepted");
        std::cout << "PASS: ManualControl snapshot commit and current-format-only loading\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
