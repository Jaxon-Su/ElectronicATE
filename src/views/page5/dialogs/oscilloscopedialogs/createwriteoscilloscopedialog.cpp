#include "createwriteoscilloscopedialog.h"
#include "msoseries456writedialog.h"
#include "page5scopepolicy.h"

#include <QLabel>
#include <QVBoxLayout>

namespace {
// 未配置或尚未開放的型號只有提示與 Close，不提供 Apply。
class UnavailableScopeDialog : public OscWriteDialogBase {
  public:
    explicit UnavailableScopeDialog(const QString& reason, QWidget* parent)
        : OscWriteDialogBase(nullptr, {}, {}, 0, {}, parent), m_reason(reason)
    {
        setWindowTitle("Write Oscilloscope — Unavailable");
        buildFrame({460, 240}, false);
    }

  protected:
    QWidget* buildContentWidget() override
    {
        auto* card = new QFrame;
        card->setObjectName("card");
        auto* layout = new QVBoxLayout(card);
        auto* message = new QLabel(m_reason);
        message->setWordWrap(true);
        message->setAlignment(Qt::AlignCenter);
        layout->addWidget(message);
        return card;
    }

  private:
    QString m_reason;
};
} // namespace

OscWriteDialogBase* createWriteOscilloscopeDialog(Oscilloscope* scope, const QString& configuredModel,
                                                  const QVariantMap& initCfg, int seqNo,
                                                  const QString& extName, QWidget* parent)
{
    const QString reason = Page5ScopePolicy::unavailableReason(configuredModel);
    if (!reason.isEmpty())
        return new UnavailableScopeDialog(reason, parent);

    return new MSOSeries456WriteDialog(scope, configuredModel, initCfg, seqNo, extName, parent);
}
