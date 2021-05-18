#include "qlogocontroller.h"
#include <QMessageBox>
#include <QByteArray>
#include <QDataStream>
#include <unistd.h>

#ifdef _WIN32
// For setmode(..., O_BINARY)
#include <fcntl.h>
#endif

// Wrapper function for sending data to the GUI
void sendMessage(std::function<void (QDataStream*)> func)
{
    qint64 datawritten;
    QByteArray buffer;
    QDataStream bufferStream(&buffer, QIODevice::WriteOnly);
    func(&bufferStream);
    qint64 datalen = buffer.size();
    buffer.prepend((const char *)&datalen, sizeof(qint64));
    datawritten = write(STDOUT_FILENO, buffer.constData(), buffer.size());
    Q_ASSERT(datawritten == buffer.size());
}


QLogoController::QLogoController(QObject *parent) : Controller(parent)
{
#ifdef _WIN32
    // That dreaded \r\n <-> \n problem
    setmode(STDOUT_FILENO, O_BINARY);
    setmode(STDIN_FILENO, O_BINARY);
#endif
}


QLogoController::~QLogoController()
{
    setDribble("");

}

void QLogoController::initialize()
{
    sendMessage([&](QDataStream *out) {
      *out << (message_t)W_INITIALIZE;
    });
    waitForMessage(W_INITIALIZE);

}

/* a message has three parts:
 * 1. A quint detailing how many bytes are in the remainder of the message (datalen).
 * 2. An enum describing the type of data (header).
 * 3. The data (varies).
 */
message_t QLogoController::getMessage()
{
    qint64 datalen;
    qint64 dataread;
    message_t header;
    do {
        dataread = read(STDIN_FILENO, &datalen, sizeof(qint64));
        if (dataread == 0) {
            QThread::msleep(100);
        }
    } while(dataread == 0);
    Q_ASSERT(dataread == sizeof(qint64));
    QByteArray buffer;
    buffer.resize(datalen);
    dataread = read(STDIN_FILENO, buffer.data(), datalen);
    Q_ASSERT(dataread == datalen);
    QDataStream bufferStream(&buffer, QIODevice::ReadOnly);

    bufferStream >> header;

    switch (header) {
    case W_ZERO:
        qDebug() <<"ZERO!";
        break;
    case W_INITIALIZE:
    {
        bufferStream >> allFontNames
                     >> textFont
                     >> minPensize
                     >> maxPensize
                     >> xbound
                     >> ybound
                ;
        qDebug() << textFont;
        labelFont = textFont;
        break;
    }
    case C_CONSOLE_RAWLINE_READ:
        bufferStream >> rawLine;
        break;
    case C_CONSOLE_CHAR_READ:
        bufferStream >> rawChar;
        break;
    default:
        qDebug() <<"I don't know how I got " << header;
        break;
    }
    return header;
}


void QLogoController::waitForMessage(message_t expectedType)
{
    message_t type;
    do {
        type = getMessage();
    } while (type != expectedType);
}


void QLogoController::printToConsole(const QString &s)
{
    if (writeStream == NULL) {
        sendMessage([&](QDataStream *out) {
          *out << (message_t)C_CONSOLE_PRINT_STRING << s;
        });

      if (dribbleStream)
        *dribbleStream << s;
    } else {
      *writeStream << s;
    }
}

// TODO: I believe this is only called if the input readStream is NULL
DatumP QLogoController::readRawlineWithPrompt(const QString &prompt)
{
  sendMessage([&](QDataStream *out) {
    *out << (message_t)C_CONSOLE_PRINT_STRING << prompt;
  });
  sendMessage([&](QDataStream *out) {
    *out << (message_t)C_CONSOLE_REQUEST_LINE;
  });
  if (dribbleStream)
    *dribbleStream << prompt;

  waitForMessage(C_CONSOLE_RAWLINE_READ);

  return DatumP(new Word(rawLine));
}


DatumP QLogoController::readchar()
{
  sendMessage([&](QDataStream *out) {
    *out << (message_t)C_CONSOLE_REQUEST_CHAR;
  });

  waitForMessage(C_CONSOLE_CHAR_READ);

  return DatumP(new Word(rawChar));
}

void QLogoController::setTurtlePos(const QMatrix4x4 &newTurtlePos)
{
  sendMessage([&](QDataStream *out) {
    *out << (message_t)C_CANVAS_UPDATE_TURTLE_POS << newTurtlePos;
  });
}

void QLogoController::drawLine(const QVector3D &start, const QVector3D &end, const QColor &color)
{
  sendMessage([&](QDataStream *out) {
    *out << (message_t)C_CANVAS_DRAW_LINE
         << start
         << end
         << color;
  });
}

void QLogoController::drawPolygon(const QList<QVector3D> &points, const QList<QColor> &colors)
{
    sendMessage([&](QDataStream *out) {
      *out << (message_t)C_CANVAS_DRAW_POLYGON
           << points
           << colors;
    });
}

// TODO: We are having a problem with the font
void QLogoController::drawLabel(const QString &aString, const QVector3D &aPosition, const QColor &aColor,
                                const QFont &aFont)
{
    sendMessage([&](QDataStream *out) {
      *out << (message_t)C_CANVAS_DRAW_LABEL
           << aString
           << aPosition
           << aColor
           << aFont;
    });
}

void QLogoController::setCanvasBackgroundColor(QColor aColor)
{
    sendMessage([&](QDataStream *out) {
      *out << (message_t)C_CANVAS_SET_BACKGROUND_COLOR
           << aColor;
    });
}

void QLogoController::clearScreen()
{
    sendMessage([&](QDataStream *out) {
      *out << (message_t)C_CANVAS_CLEAR_SCREEN;
    });
}

void QLogoController::setBounds(double x, double y)
{
    if ((xbound == x) && (ybound == y))
        return;
    xbound = x;
    ybound = y;
    sendMessage([&](QDataStream *out) {
      *out << (message_t)C_CANVAS_SETBOUNDS
           << xbound
           << ybound;
    });

}

void QLogoController::setPensize(double aSize)
{
    if (aSize == penSize)
        return;
    sendMessage([&](QDataStream *out) {
      *out << (message_t)C_CANVAS_SET_PENSIZE
           << aSize;
    });
    penSize = aSize;
}
