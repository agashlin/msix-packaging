//
//  Copyright (C) 2019 Microsoft.  All rights reserved.
//  See LICENSE file in the project root for full license information.
// 

#include "XmlWriter.hpp"
#include "ContentTypeWriter.hpp"
#include "StreamHelper.hpp"
#include "AppxFactory.hpp"

#include <map>
#include <algorithm>

namespace MSIX {

    /*
    <Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
      <Default ContentType="image/png" Extension="png"/>
      <Default ContentType="application/x-msdownload" Extension="dll"/>
      <Default ContentType="application/vnd.ms-appx.manifest+xml" Extension="xml"/>
      <Override ContentType="application/vnd.ms-appx.blockmap+xml" PartName="/AppxBlockMap.xml"/>
      <Override ContentType="application/vnd.ms-appx.signature" PartName="/AppxSignature.p7x"/>
      <Override ContentType="application/vnd.ms-pkiseccat" PartName="/AppxMetadata/CodeIntegrity.cat"/>
    </Types>
    */

    static const char* typesElement = "Types";
    static const char* typesNamespace = "http://schemas.openxmlformats.org/package/2006/content-types";
    static const char* defaultElement = "Default";
    static const char* contentTypeAttribute = "ContentType";
    static const char* extensionAttribute = "Extension";
    static const char* overrideElement = "Override";
    static const char* partNameAttribute = "PartName";

    // <Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
    ContentTypeWriter::ContentTypeWriter() : m_xmlWriter(typesElement, true)
    {
        m_xmlWriter.AddAttribute(xmlnsAttribute, typesNamespace);
    }

    ContentTypeWriter::ContentTypeWriter(IStream* stream)
    {
        // Check to see if we already have signing content types
        std::string sourceXml = Helper::CreateStringFromStream(stream);

        // Determine if the signature file overrides are already present
        std::string signaturePartNameSearch = GetPartNameSearchString(APPXSIGNATURE_P7X);
        std::string ciPartNameSearch = GetPartNameSearchString(CODEINTEGRITY_CAT);
        m_hasSignatureOverride = (sourceXml.rfind(signaturePartNameSearch) != std::string::npos);
        m_hasCIOverride = (sourceXml.rfind(ciPartNameSearch) != std::string::npos);

        m_xmlWriter.Initialize(sourceXml, typesElement);
    }

    void ContentTypeWriter::AddContentType(const std::string& name, const std::string& contentType, bool forceOverride)
    {
        // Skip the signature files if they are already present
        if ((name == APPXSIGNATURE_P7X && m_hasSignatureOverride) ||
            (name == CODEINTEGRITY_CAT && m_hasCIOverride))
        {
            return;
        }

        if (forceOverride)
        {
            AddOverride(name, contentType);
            return;
        }

        // See if already exist
        std::string ext = name.substr(name.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        auto find = m_defaultExtensions.find(ext);
        if (find != m_defaultExtensions.end())
        {
            if (find->second != contentType)
            {
                // The extension is in the table but with a different content type
                AddOverride(name, contentType);
            }
        }
        else
        {
            AddDefault(ext, contentType);
            m_defaultExtensions.emplace(ext, contentType);
        }
    }

    void ContentTypeWriter::Close()
    {
        m_xmlWriter.CloseElement();
        ThrowErrorIf(Error::Unexpected, m_xmlWriter.GetState() != XmlWriter::Finish, "Content Type xml didn't close correctly");
    }

    // <Default ContentType="application/vnd.ms-appx.manifest+xml" Extension="xml"/>
    void ContentTypeWriter::AddDefault(const std::string& ext, const std::string& contentType)
    {
        m_xmlWriter.StartElement(defaultElement);
        m_xmlWriter.AddAttribute(contentTypeAttribute, contentType);
        m_xmlWriter.AddAttribute(extensionAttribute, ext);
        m_xmlWriter.CloseElement();
    }

    // <Override ContentType="application/vnd.ms-appx.signature" PartName="/AppxSignature.p7x"/>
    void ContentTypeWriter::AddOverride(const std::string& file, const std::string& contentType)
    {
        std::string partName = "/" + file;
        m_xmlWriter.StartElement(overrideElement);
        m_xmlWriter.AddAttribute(contentTypeAttribute, contentType);
        m_xmlWriter.AddAttribute(partNameAttribute, partName);
        m_xmlWriter.CloseElement();
    }

    // Gets the search string from a file name; AppxSignature.p7x => "/AppxSignature.p7x"
    std::string ContentTypeWriter::GetPartNameSearchString(const std::string& fileName)
    {
        return "\"/" + fileName + '"';
    }
}